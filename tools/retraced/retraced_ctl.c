/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The controller plane's command surface (extracted from
 * main.c -- the architecture-review testability candidate): one
 * newline-JSON line in, one reply line out through the injected
 * sink. No socket, no daemon, no poll loop -- unit-testable.
 */

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "retraced_ctl.h"
#include "tls_gate.h"
#include "parson.h"

/*
 * write_frame lives in main.c's TU (the connection-plane
 * writer); declared here for the policy push
 */
int write_frame(int fd, uint16_t type, const char *payload);

void retraced_ctl_set_policy(struct retraced_ctl_ctx *ctx,
	const char *blob, long epoch)
{
	free(ctx->policy_blob);
	ctx->policy_blob = strdup(blob);
	ctx->policy_epoch = epoch;
}

int retraced_ctl_push_policy(struct retraced_ctl_ctx *ctx,
	const char *blob)
{
	int i, pushed = 0;

	for (i = 0; i < MAX_AGENTS; i++) {
		if (ctx->conns[i].fd >= 0 &&
		    ctx->conns[i].helloed &&
		    !ctx->conns[i].spectator) {
			write_frame(ctx->conns[i].fd,
				RETRACE_RPC_MSG_POLICY_SET, blob);
			pushed++;
		}
	}
	return pushed;
}

static void ctl_reply(struct retraced_ctl_ctx *ctx,
	const char *fmt, ...)
{
	char out[1024];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(out, sizeof(out), fmt, ap);
	va_end(ap);
	if (n > 0 && ctx->reply_sink != NULL)
		ctx->reply_sink(out, ctx->reply_user);
}

void retraced_ctl_handle_line(struct retraced_ctl_ctx *ctx,
	char *line)
{
	JSON_Value *v = json_parse_string(line);
	JSON_Object *o;
	const char *cmd;

	if (v == NULL) {
		ctl_reply(ctx, "{\"ok\":0,\"error\":\"malformed json\"}\n");
		return;
	}
	o = json_value_get_object(v);
	cmd = json_object_get_string(o, "cmd");
	if (cmd == NULL) {
		ctl_reply(ctx, "{\"ok\":0,\"error\":\"no cmd\"}\n");
		json_value_free(v);
		return;
	}
	/*
	 * Scope gate (TODO.supervisor/08 P1): every command maps
	 * to a claim bit; a TLS peer whose cert URI SAN does not
	 * carry it is refused + journaled. Local UDS peers hold
	 * RETRACED_SCOPE_ALL (set at accept).
	 */
	{
		uint32_t need = retraced_tls_scope_for_cmd(cmd);

		if (need == 0) {
			ctl_reply(ctx, "{\"ok\":0,\"error\":\"unknown cmd\"}\n");
			json_value_free(v);
			return;
		}
		if ((ctx->scopes & need) != need) {
			char ev[160];

			snprintf(ev, sizeof(ev),
				"{\"name\":\"retrace.auth.overscope\",\"cmd\":\"%s\",\"have\":%u,\"need\":%u}",
				cmd, (unsigned int)ctx->scopes, (unsigned int)need);
			retraced_journal_event(ctx->jr, (long)time(NULL),
				"daemon", 0, ev);
			ctl_reply(ctx,
				"{\"ok\":0,\"error\":\"scope denied\"}\n");
			json_value_free(v);
			return;
		}
	}
	if (strcmp(cmd, "status") == 0) {
		ctl_reply(ctx, "{\"ok\":1,\"pid\":%ld,\"agents\":%zu,"
			"\"policy_epoch\":%ld,\"frozen\":%d}\n",
			(long)getpid(), ctx->reg->count, ctx->policy_epoch,
			ctx->frozen);
	} else if (strcmp(cmd, "ps") == 0) {
		JSON_Value *snap = retraced_registry_to_json(ctx->reg);
		char *s = json_serialize_to_string(snap);

		ctl_reply(ctx, "{\"ok\":1,\"registry\":%s}\n",
			s != NULL ? s : "null");
		json_free_serialized_string(s);
		json_value_free(snap);
	} else if (strcmp(cmd, "policy_push") == 0) {
		const char *blob = json_object_get_string(o, "blob");
		JSON_Value *pv;
		JSON_Object *po, *proot;
		double epoch;
		int pushed;

		if (blob == NULL) {
			ctl_reply(ctx, "{\"ok\":0,\"error\":\"no blob\"}\n");
			json_value_free(v);
			return;
		}
		pv = json_parse_string(blob);
		proot = pv != NULL ? json_value_get_object(pv) : NULL;
		/* signed policies: validate inside the wrapper's blob */
		if (proot != NULL &&
		    json_object_get_string(proot, "blob") != NULL) {
			JSON_Value *bv = json_parse_string(
				json_object_get_string(proot, "blob"));

			if (bv != NULL) {
				json_value_free(pv);
				pv = bv;
				proot = json_value_get_object(bv);
			}
		}
		po = proot != NULL ?
			json_object_get_object(proot, "policy") : NULL;
		epoch = po != NULL ?
			json_object_get_number(po, "epoch") : 0.0;
		if (epoch < 1.0 || json_object_get_array(proot,
			    "intercept_scripts") == NULL) {
			ctl_reply(ctx, "{\"ok\":0,\"error\":\"bad policy"
				" (need policy.epoch + scripts)\"}\n");
			json_value_free(pv);
			json_value_free(v);
			return;
		}
		retraced_ctl_set_policy(ctx, blob, (long)epoch);
		ctx->frozen = 0;
		free(ctx->thaw_blob);
		ctx->thaw_blob = strdup(blob);
		pushed = retraced_ctl_push_policy(ctx, blob);
		{
			char ev[160];

			snprintf(ev, sizeof(ev),
				"{\"name\":\"retrace.policy.pushed\",\"epoch\":%ld,\"agents\":%d}",
				(long)epoch, pushed);
			retraced_journal_event(ctx->jr, (long)time(NULL),
				"daemon", 0, ev);
		}
		ctl_reply(ctx, "{\"ok\":1,\"epoch\":%ld,\"pushed\":%d}\n",
			(long)epoch, pushed);
		json_value_free(pv);
	} else if (strcmp(cmd, "freeze") == 0) {
		char blob[256];
		long epoch = ctx->policy_epoch + 1;
		int pushed;

		snprintf(blob, sizeof(blob),
			"{\"policy\":{\"epoch\":%ld},"
			"\"intercept_scripts\":[{\"func_name\":\"*\","
			"\"actions\":[{\"action_name\":\"freeze\"}]}]}",
			epoch);
		retraced_ctl_set_policy(ctx, blob, epoch);
		ctx->frozen = 1;
		pushed = retraced_ctl_push_policy(ctx, blob);
		{
			char ev[128];

			snprintf(ev, sizeof(ev),
				"{\"name\":\"retrace.policy.freeze\",\"epoch\":%ld,\"agents\":%d}",
				epoch, pushed);
			retraced_journal_event(ctx->jr, (long)time(NULL),
				"daemon", 0, ev);
		}
		ctl_reply(ctx, "{\"ok\":1,\"epoch\":%ld,\"pushed\":%d}\n",
			epoch, pushed);
	} else if (strcmp(cmd, "thaw") == 0) {
		long epoch = ctx->policy_epoch + 1;
		char thawed[8192];
		const char *src = ctx->thaw_blob != NULL ? ctx->thaw_blob :
			"{\"policy\":{\"epoch\":1},\"intercept_scripts\":[]}";
		JSON_Value *tv = json_parse_string(src);
		int pushed;

		/* re-stamp the saved policy at a fresh epoch: agents
		 * only ever accept strictly-greater epochs
		 */
		if (tv != NULL) {
			JSON_Object *troot = json_value_get_object(tv);
			JSON_Object *tpo = json_object_get_object(troot,
				"policy");

			if (tpo != NULL)
				json_object_set_number(tpo, "epoch",
					(double)epoch);
		}
		{
			char *s = json_serialize_to_string(tv);

			snprintf(thawed, sizeof(thawed), "%s",
				s != NULL ? s : src);
			json_free_serialized_string(s);
		}
		json_value_free(tv);
		retraced_ctl_set_policy(ctx, thawed, epoch);
		ctx->frozen = 0;
		pushed = retraced_ctl_push_policy(ctx, thawed);
		{
			char ev[128];

			snprintf(ev, sizeof(ev),
				"{\"name\":\"retrace.policy.thaw\",\"epoch\":%ld,\"agents\":%d}",
				epoch, pushed);
			retraced_journal_event(ctx->jr, (long)time(NULL),
				"daemon", 0, ev);
		}
		ctl_reply(ctx, "{\"ok\":1,\"epoch\":%ld,\"pushed\":%d}\n",
			epoch, pushed);
	} else if (strcmp(cmd, "kill") == 0) {
		long pid = (long)json_object_get_number(o, "pid");

		if (pid <= 0) {
			ctl_reply(ctx, "{\"ok\":0,\"error\":\"no pid\"}\n");
			json_value_free(v);
			return;
		}
		kill((pid_t)pid, SIGTERM);
		{
			char ev[128];

			snprintf(ev, sizeof(ev),
				"{\"name\":\"retrace.ctl.kill\",\"pid\":%ld}",
				pid);
			retraced_journal_event(ctx->jr, (long)time(NULL),
				"daemon", 0, ev);
		}
		ctl_reply(ctx, "{\"ok\":1,\"pid\":%ld}\n", pid);
	} else {
		ctl_reply(ctx, "{\"ok\":0,\"error\":\"unknown cmd\"}\n");
	}
	json_value_free(v);
}

