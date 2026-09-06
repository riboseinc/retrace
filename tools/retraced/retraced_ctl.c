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
#include "ctl_verbs.h"
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

/*
 * The verb table: the SSOT list (ctl_verbs.h) expands to name,
 * scope, and handler -- dispatch, the scope gate, and the CLI
 * usage all derive from the same rows (the X-macro idiom the
 * protocol table set). A verb is one list line plus one
 * handler; nothing else enumerates names.
 */
struct retraced_ctl_verb {
	const char *name;
	uint32_t scope;
	void (*fn)(struct retraced_ctl_ctx *ctx, JSON_Object *o);
};

#define VERB_FWD(n, c, s, a, h)					      \
	static void verb_##n(struct retraced_ctl_ctx *ctx,	      \
		JSON_Object *o);
RETRACED_CTL_VERBS(VERB_FWD)

#define VERB_ROW(n, c, s, a, h)					      \
	{ #n, RETRACED_SCOPE_##s, verb_##n },
static const struct retraced_ctl_verb ctl_verbs[] = {
	RETRACED_CTL_VERBS(VERB_ROW)
};

static const struct retraced_ctl_verb *ctl_verb_find(
	const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(ctl_verbs) / sizeof(ctl_verbs[0]);
	     i++) {
		if (strcmp(ctl_verbs[i].name, name) == 0)
			return &ctl_verbs[i];
	}
	return NULL;
}

uint32_t retraced_tls_scope_for_cmd(const char *cmd)
{
	const struct retraced_ctl_verb *vb = cmd != NULL ?
		ctl_verb_find(cmd) : NULL;

	return vb != NULL ? vb->scope : 0;
}

/*
 * Nest `child` under the node whose id is `parent_id`, at any
 * depth. Returns 1 placed. The session tree's whole point is
 * depth: worker trees and fork-bomb detonations chain past any
 * fixed scan level (the depth-2 shortcut this replaced
 * flattened their tails at the root).
 */
static int place_agent(JSON_Object *node, const char *parent_id,
	JSON_Value *child)
{
	JSON_Array *children;
	size_t i;

	if (strcmp(json_object_get_string(node, "id"), parent_id) == 0) {
		json_array_append_value(json_value_get_array(
			json_object_get_value(node, "children")), child);
		return 1;
	}
	children = json_value_get_array(
		json_object_get_value(node, "children"));
	for (i = 0; i < json_array_get_count(children); i++) {
		if (place_agent(json_array_get_object(children, i),
			    parent_id, child))
			return 1;
	}
	return 0;
}

struct ev_sink_ctx {
	char *p;
	size_t left;
	int first;
};

static void ev_sink_append(const char *line, void *user)
{
	struct ev_sink_ctx *sc = (struct ev_sink_ctx *)user;
	size_t n;

	if (!sc->first && sc->left > 1) {
		*sc->p++ = ',';
		sc->left--;
	}
	sc->first = 0;
	n = strlen(line);
	if (n > sc->left - 1)
		n = sc->left - 1;
	memcpy(sc->p, line, n);
	sc->p += n;
	sc->left -= n;
}

void retraced_ctl_handle_line(struct retraced_ctl_ctx *ctx,
	char *line)
{
	JSON_Value *v = json_parse_string(line);
	JSON_Object *o;
	const char *cmd;
	const struct retraced_ctl_verb *vb;

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
	vb = ctl_verb_find(cmd);
	if (vb == NULL) {
		ctl_reply(ctx, "{\"ok\":0,\"error\":\"unknown cmd\"}\n");
		json_value_free(v);
		return;
	}
	/*
	 * Scope gate (TODO.supervisor/08 P1): the table carries
	 * each verb's claim bit; a peer whose cert lacks it is
	 * refused + journaled. Local UDS peers hold
	 * RETRACED_SCOPE_ALL (set at accept).
	 */
	if ((ctx->scopes & vb->scope) != vb->scope) {
		char ev[160];

		snprintf(ev, sizeof(ev),
			"{\"name\":\"retrace.auth.overscope\",\"cmd\":\"%s\",\"have\":%u,\"need\":%u}",
			cmd, (unsigned int)ctx->scopes,
			(unsigned int)vb->scope);
		retraced_journal_event(ctx->jr, (long)time(NULL),
			"daemon", 0, ev);
		ctl_reply(ctx, "{\"ok\":0,\"error\":\"scope denied\"}\n");
		json_value_free(v);
		return;
	}
	/* the dispatcher owns the parse tree: handlers reply and
	 * return; every path frees exactly once, here
	 */
	vb->fn(ctx, o);
	json_value_free(v);
}

static void verb_status(struct retraced_ctl_ctx *ctx,
	JSON_Object *o)
{

	ctl_reply(ctx, "{\"ok\":1,\"pid\":%ld,\"agents\":%zu,"
		"\"policy_epoch\":%ld,\"frozen\":%d}\n",
		(long)getpid(), ctx->reg->count, ctx->policy_epoch,
		ctx->frozen);
}

static void verb_ps(struct retraced_ctl_ctx *ctx,
	JSON_Object *o)
{
	JSON_Value *snap = retraced_registry_to_json(ctx->reg);
	char *s = json_serialize_to_string(snap);

	ctl_reply(ctx, "{\"ok\":1,\"registry\":%s}\n",
		s != NULL ? s : "null");
	json_free_serialized_string(s);
	json_value_free(snap);
}

static void verb_sessions(struct retraced_ctl_ctx *ctx,
	JSON_Object *o)
{
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

			for (k = 0; k < json_array_get_count(
				     agents) && !placed; k++) {
				placed = place_agent(
					json_array_get_object(
						agents, k),
					e->parent_id, av);
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
}

static void verb_events(struct retraced_ctl_ctx *ctx,
	JSON_Object *o)
{
	/*
	 * The evidence read arm: the journal's last records,
	 * chain verdict riding the reply -- evidence pulled
	 * over a network carries its own integrity statement.
	 * last is bounded (<=128, default 20). Observe-only:
	 * the STATUS claim, the least-privilege scope an
	 * auditor's certificate needs.
	 */
	double last_d = json_object_get_number(o, "last");
	size_t last_n = last_d > 0 ? (size_t)last_d : 20;
	long chain = 0;

	if (last_n > 128) {
		ctl_reply(ctx,
			"{\"ok\":0,\"error\":\"last > 128\"}\n");
		return;
	}
	{
		char *buf;
		size_t cap = last_n * 2100 + 64;
		struct ev_sink_ctx sc;
		int n;

		buf = (char *)malloc(cap);
		if (buf == NULL) {
			ctl_reply(ctx,
				"{\"ok\":0,\"error\":\"oom\"}\n");
			return;
		}
		sc.p = buf;
		sc.left = cap;
		sc.first = 1;
		n = retraced_journal_tail(ctx->jr, last_n,
			ev_sink_append, &sc, &chain);
		(void)n;
		ctl_reply(ctx,
			"{\"ok\":1,\"chain\":\"%s\",\"broken_at\":%ld,\"events\":[%.*s]}\n",
			chain == 0 ? "verified" : "broken",
			chain,
			(int)(cap - sc.left), buf);
		free(buf);
	}
}

static void verb_spawn(struct retraced_ctl_ctx *ctx,
	JSON_Object *o)
{
	/*
	 * The reserved verb (the threat model's host
	 * process control): launch a workload armed to
	 * join THIS daemon -- supervisor env, agent
	 * socket, nonce, preload -- and journal the
	 * action with argv, kill's audit pattern
	 * extended. SPAWN claim required (the scope gate
	 * above); the transport's spawn seam does the
	 * launching (NULL on Windows: injection is
	 * retrace-win-run's machinery, not a stub here).
	 */
	JSON_Array *argv_a = json_object_get_array(o, "argv");
	const char *preload =
		json_object_get_string(o, "preload");

	if (ctx->spawn_cb == NULL) {
		ctl_reply(ctx,
			"{\"ok\":0,\"error\":\"spawn: not on this platform -- use retrace-win-run\"}\n");
		return;
	}
	if (argv_a == NULL || json_array_get_count(argv_a) == 0) {
		ctl_reply(ctx,
			"{\"ok\":0,\"error\":\"no argv\"}\n");
		return;
	}
	{
		char *argv_buf[129];
		char err[96];
		size_t n = json_array_get_count(argv_a);
		size_t ai;
		long pid;

		if (n > 128) {
			ctl_reply(ctx,
				"{\"ok\":0,\"error\":\"argv > 128\"}\n");
			return;
		}
		for (ai = 0; ai < n; ai++)
			argv_buf[ai] = (char *)
				json_array_get_string(argv_a, ai);
		argv_buf[n] = NULL;
		err[0] = '\0';
		pid = ctx->spawn_cb(
			(const char *const *)argv_buf, preload,
			err, sizeof(err));
		if (pid <= 0) {
			ctl_reply(ctx,
				"{\"ok\":0,\"error\":\"spawn failed%s%s\"}\n",
				err[0] != '\0' ? ": " : "", err);
			return;
		}
		{
			char ev[256];

			snprintf(ev, sizeof(ev),
				"{\"name\":\"retrace.ctl.spawn\",\"pid\":%ld,\"argv0\":\"%s\"}",
				pid,
				json_array_get_string(argv_a, 0) !=
					NULL ?
					json_array_get_string(
						argv_a, 0) : "");
			retraced_journal_event(ctx->jr,
				(long)time(NULL), "daemon", 0, ev);
		}
		ctl_reply(ctx, "{\"ok\":1,\"pid\":%ld}\n",
			pid);
	}
}

static void verb_policy_push(struct retraced_ctl_ctx *ctx,
	JSON_Object *o)
{
	const char *in_blob = json_object_get_string(o, "blob");
	char *blob = NULL;
	long epoch = 0;
	int pushed;

	if (in_blob == NULL) {
		ctl_reply(ctx, "{\"ok\":0,\"error\":\"no blob\"}\n");
		return;
	}
	if (retraced_policy_load(in_blob, &blob, &epoch) != 0) {
		ctl_reply(ctx, "{\"ok\":0,\"error\":\"bad policy"
			" (need policy.epoch + scripts)\"}\n");
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
}

static void verb_freeze(struct retraced_ctl_ctx *ctx,
	JSON_Object *o)
{
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
}

static void verb_thaw(struct retraced_ctl_ctx *ctx,
	JSON_Object *o)
{
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
}

static void verb_kill(struct retraced_ctl_ctx *ctx,
	JSON_Object *o)
{
	long pid = (long)json_object_get_number(o, "pid");

	if (pid <= 0) {
		ctl_reply(ctx, "{\"ok\":0,\"error\":\"no pid\"}\n");
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
}

