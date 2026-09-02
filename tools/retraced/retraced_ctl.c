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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include "retraced_ctl.h"
#include "tls_gate.h"
#include "parson.h"

int retraced_policy_load(const char *text, char **blob_out,
	long *epoch_out)
{
	JSON_Value *v = json_parse_string(text);
	JSON_Object *root;
	const char *blob = text;
	double epoch = 0;

	*blob_out = NULL;
	if (epoch_out != NULL)
		*epoch_out = 0;
	if (v == NULL)
		return -1;
	root = json_value_get_object(v);
	/* signed policies: the wrapper carries the policy in blob */
	if (root != NULL && json_object_get_string(root, "blob") != NULL) {
		JSON_Value *bv = json_parse_string(
			json_object_get_string(root, "blob"));

		if (bv == NULL) {
			json_value_free(v);
			return -1;
		}
		json_value_free(v);
		v = bv;
		root = json_value_get_object(bv);
		blob = json_object_get_string(root, "blob");
	}
	if (root != NULL) {
		JSON_Object *pol = json_object_get_object(root,
			"policy");

		if (pol != NULL)
			epoch = json_object_get_number(pol, "epoch");
	}
	if (epoch < 1.0 ||
	    (root != NULL &&
	     json_object_get_array(root, "intercept_scripts")
		     == NULL)) {
		json_value_free(v);
		return -1;
	}
	*blob_out = strdup(text);
	if (*blob_out == NULL) {
		json_value_free(v);
		return -1;
	}
	if (epoch_out != NULL)
		*epoch_out = (long)epoch;
	json_value_free(v);
	return 0;
}

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
		if (ctx->conns[i].io != NULL &&
		    ctx->conns[i].helloed &&
		    !ctx->conns[i].spectator &&
		    ctx->conn_send != NULL) {
			ctx->conn_send(ctx->conns[i].io,
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

uint32_t retraced_tls_scope_for_cmd(const char *cmd)
{
	if (cmd == NULL)
		return 0;
	if (strcmp(cmd, "status") == 0)
		return RETRACED_SCOPE_STATUS;
	if (strcmp(cmd, "ps") == 0 || strcmp(cmd, "sessions") == 0)
		return RETRACED_SCOPE_PS;
	if (strcmp(cmd, "policy_push") == 0 ||
	    strcmp(cmd, "freeze") == 0 ||
	    strcmp(cmd, "thaw") == 0)
		return RETRACED_SCOPE_POLICY;
	if (strcmp(cmd, "kill") == 0)
		return RETRACED_SCOPE_KILL;
	if (strcmp(cmd, "spawn") == 0)
		return RETRACED_SCOPE_SPAWN;
	return 0;
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
	} else if (strcmp(cmd, "sessions") == 0) {
		/*
		 * The session tree: the registry already carries
		 * it (session token, parent agent id, spectator
		 * seats) -- ps prints it flat and the operator
		 * re-nests by eye. Group once, here: per session
		 * token, agents nested by parent, spectators and
		 * parent-holes marked. Pure transform of the same
		 * data ps serializes.
		 */
		JSON_Value *root = json_value_init_object();
		JSON_Object *root_o = json_value_get_object(root);
		JSON_Value *sessions_val = json_value_init_array();
		JSON_Array *sessions =
			json_value_get_array(sessions_val);
		size_t i, k;

		/* one pass: collect the distinct session tokens */
		for (i = 0; i < ctx->reg->count; i++) {
			const struct agent_entry *e =
				&ctx->reg->agents[i];
			int seen = 0;

			for (k = 0; k < json_array_get_count(sessions);
			     k++) {
				const char *tok = json_object_get_string(
					json_array_get_object(sessions, k),
					"token");

				if (tok != NULL &&
				    strcmp(tok, e->session) == 0) {
					seen = 1;
					break;
				}
			}
			if (!seen) {
				JSON_Value *sv = json_value_init_object();

				json_object_set_string(
					json_value_get_object(sv),
					"token", e->session);
				json_object_set_value(
					json_value_get_object(sv),
					"agents", json_value_init_array());
				json_array_append_value(sessions, sv);
			}
		}

		/*
		 * nest each agent under its session, parents first
		 * (the registry appends in arrival order; a parent
		 * HELLOs before its fork children in practice, and a
		 * child whose parent is not yet present nests at the
		 * root with parent_hole marked -- the same honesty
		 * the journal carries)
		 */
		for (i = 0; i < ctx->reg->count; i++) {
			struct agent_entry *e = &ctx->reg->agents[i];
			JSON_Object *sess = NULL;
			JSON_Array *agents = NULL;
			JSON_Value *av = json_value_init_object();
			JSON_Object *ao = json_value_get_object(av);

			for (k = 0; k < json_array_get_count(sessions);
			     k++) {
				JSON_Object *cand = json_array_get_object(
					sessions, k);

				if (strcmp(json_object_get_string(cand,
					    "token"), e->session) == 0) {
					sess = cand;
					break;
				}
			}
			if (sess == NULL) {
				json_value_free(av);
				continue;
			}
			agents = json_value_get_array(
				json_object_get_value(sess, "agents"));

			json_object_set_string(ao, "id", e->id);
			json_object_set_string(ao, "cmdline",
				e->cmdline);
			json_object_set_number(ao, "pid", (double)e->pid);
			json_object_set_number(ao, "spectator",
				(double)e->spectator);
			if (e->parent_hole)
				json_object_set_number(ao,
					"parent_hole", 1);
			json_object_set_value(ao, "children",
				json_value_init_array());

			if (e->parent_id[0] != '\0') {
				int placed = 0;
				/*
				 * find the parent anywhere under this
				 * session (the tree is shallow: walk
				 * roots, then children, one level of
				 * recursion through a helper below)
				 */
				JSON_Array *roots = agents;

				for (k = 0; k < json_array_get_count(
					     roots) && !placed; k++) {
					JSON_Object *cand = json_array_get_object(
						roots, k);

					if (strcmp(json_object_get_string(
						    cand, "id"),
						    e->parent_id) == 0) {
						json_array_append_value(
							json_value_get_array(
								json_object_get_value(
									cand, "children")),
							av);
						placed = 1;
					} else {
						JSON_Array *grand = json_value_get_array(
							json_object_get_value(
								cand,
								"children"));

						for (size_t g = 0; grand !=
							NULL && g <
							json_array_get_count(
								grand) &&
							!placed; g++) {
							JSON_Object *gc =
								json_array_get_object(
									grand, g);

							if (strcmp(
								 json_object_get_string(
									 gc, "id"),
								 e->parent_id) ==
								0) {
								json_array_append_value(
									json_value_get_array(
										json_object_get_value(
											gc,
											"children")),
									av);
								placed = 1;
							}
						}
					}
				}
				if (!placed)
					json_array_append_value(agents, av);
			} else {
				json_array_append_value(agents, av);
			}
		}

		json_object_set_number(root_o, "ok", 1);
		json_object_set_number(root_o, "sessions_count",
			(double)json_array_get_count(sessions));
		json_object_set_value(root_o, "sessions", sessions_val);
		{
			char *s = json_serialize_to_string(root);

			ctl_reply(ctx, "%s\n", s != NULL ? s : "{}");
			json_free_serialized_string(s);
		}
		json_value_free(root);
	} else if (strcmp(cmd, "policy_push") == 0) {
		const char *in_blob = json_object_get_string(o, "blob");
		char *blob = NULL;
		long epoch = 0;
		int pushed;

		if (in_blob == NULL) {
			ctl_reply(ctx, "{\"ok\":0,\"error\":\"no blob\"}\n");
			json_value_free(v);
			return;
		}
		if (retraced_policy_load(in_blob, &blob, &epoch) != 0) {
			ctl_reply(ctx, "{\"ok\":0,\"error\":\"bad policy"
				" (need policy.epoch + scripts)\"}\n");
			json_value_free(v);
			return;
		}
		retraced_ctl_set_policy(ctx, blob, epoch);
		ctx->frozen = 0;
		free(ctx->thaw_blob);
		ctx->thaw_blob = strdup(blob);
		pushed = retraced_ctl_push_policy(ctx, blob);
		{
			char ev[160];

			snprintf(ev, sizeof(ev),
				"{\"name\":\"retrace.policy.pushed\",\"epoch\":%ld,\"agents\":%d}",
				epoch, pushed);
			retraced_journal_event(ctx->jr, (long)time(NULL),
				"daemon", 0, ev);
		}
		ctl_reply(ctx, "{\"ok\":1,\"epoch\":%ld,\"pushed\":%d}\n",
			epoch, pushed);
		free(blob);
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
#ifdef _WIN32
		{
			HANDLE h = OpenProcess(PROCESS_TERMINATE,
				FALSE, (DWORD)pid);

			if (h != NULL) {
				TerminateProcess(h, 1);
				CloseHandle(h);
			}
		}
#else
		kill((pid_t)pid, SIGTERM);
#endif
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

