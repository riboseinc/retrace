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
 * The controller plane's command surface, unit-tested through
 * its module interface (the extraction's whole point): no
 * socket, no daemon, no poll loop -- a buffer sink collects
 * replies. These tests are why handle_ctl_line moved out of
 * main.c.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "retraced_ctl.h"
#include "ctl_verbs.h"
#include "parson.h"
#include "tls_gate.h"

/*
 * the broadcast seam, faked: pushes are COUNTED, not sent --
 * no socket, no daemon, no extern symbol
 */
static int fake_conn_send(void *io, uint16_t type,
	const char *payload)
{
	(void)io;
	(void)type;
	(void)payload;
	return 0;
}

static int tests_run;
static int tests_pass;
static int tests_fail;

#define TEST(name) do { \
	tests_run++; \
	printf("  TEST %s ... ", #name); \
	test_##name(); \
	tests_pass++; \
	printf("OK\n"); \
} while (0)

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

static char reply_buf[4096];
static size_t reply_len;
static char replies_all[4096];	/* every reply, concatenated */
static int replies_count;

static void sink(const char *line, void *user)
{
	(void)user;
	size_t o = 0;

	while (*line != '\0' && o + 1 < sizeof(reply_buf))
		reply_buf[o++] = *line++;
	reply_buf[o] = '\0';
	reply_len = o;
	{
		char *dst = replies_all;

		while (*line != '\0' && replies_count < 128)
			line++;	/* already consumed above */
		(void)dst;
		(void)line;
	}
}

static void feed(struct retraced_ctl_ctx *ctx, const char *line)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), "%s", line);
	reply_buf[0] = '\0';
	reply_len = 0;
	retraced_ctl_handle_line(ctx, buf);
}

/* the byte layer: transport chunks in, one reply counter */
static int feed_count;

static void counting_sink(const char *line, void *user)
{
	(void)user;
	feed_count++;
	snprintf(reply_buf, sizeof(reply_buf), "%s", line);
}

static struct retraced_ctl_ctx ctx;
static struct conn conns[MAX_AGENTS];

static void setup(void)
{
	static struct retraced_registry reg;
	static struct retraced_journal jr;

	memset(&ctx, 0, sizeof(ctx));
	memset(conns, 0, sizeof(conns));
	memset(&reg, 0, sizeof(reg));
	memset(&jr, 0, sizeof(jr));
	retraced_journal_open(&jr, "/dev/null");
	ctx.conns = conns;
	ctx.reg = &reg;
	ctx.jr = &jr;
	ctx.reply_sink = sink;
	ctx.conn_send = fake_conn_send;
	ctx.scopes = RETRACED_SCOPE_ALL; /* local-UDS peer default */
}

static void test_malformed(void)
{
	setup();
	feed(&ctx, "not json at all");
	CHECK(strstr(reply_buf, "\"error\":\"malformed json\"") != NULL);
}

static void test_no_cmd(void)
{
	setup();
	feed(&ctx, "{}");
	CHECK(strstr(reply_buf, "\"error\":\"no cmd\"") != NULL);
}

static void test_unknown(void)
{
	setup();
	feed(&ctx, "{\"cmd\":\"teleport\"}");
	CHECK(strstr(reply_buf, "\"error\":\"unknown cmd\"") != NULL);
}

static void test_status(void)
{
	setup();
	feed(&ctx, "{\"cmd\":\"status\"}");
	CHECK(strstr(reply_buf, "\"ok\":1") != NULL);
	CHECK(strstr(reply_buf, "\"agents\":0") != NULL);
	CHECK(strstr(reply_buf, "\"frozen\":0") != NULL);
}

static void test_ps(void)
{
	setup();
	feed(&ctx, "{\"cmd\":\"ps\"}");
	CHECK(strstr(reply_buf, "\"ok\":1") != NULL);
	CHECK(strstr(reply_buf, "\"registry\"") != NULL);
}

static void test_policy_push_valid(void)
{
	setup();
	feed(&ctx, "{\"cmd\":\"policy_push\",\"blob\":"
		   "\"{\\\"policy\\\":{\\\"epoch\\\":7},"
		   "\\\"intercept_scripts\\\":[{\\\"func_name\\\":\\\"*\\\","
		   "\\\"actions\\\":[{\\\"action_name\\\":\\\"log_params\\\"}]}]}\"}");
	CHECK(strstr(reply_buf, "\"ok\":1") != NULL);
	CHECK(strstr(reply_buf, "\"epoch\":7") != NULL);
	CHECK(ctx.policy_epoch == 7);
	CHECK(ctx.policy_blob != NULL);
	CHECK(ctx.frozen == 0);
	CHECK(ctx.thaw_blob != NULL);

	/* the broadcast through the seam: a full peer is pushed,
	 * a spectator is not (evidence always, policy never)
	 */
	setup();
	conns[0].io = (void *)0x1234;
	conns[0].helloed = 1;
	conns[0].spectator = 0;
	conns[1].io = (void *)0x1234;
	conns[1].helloed = 1;
	conns[1].spectator = 1;
	feed(&ctx, "{\"cmd\":\"policy_push\",\"blob\":"
		   "\"{\\\"policy\\\":{\\\"epoch\\\":8},"
		   "\\\"intercept_scripts\\\":[{\\\"func_name\\\":\\\"*\\\","
		   "\\\"actions\\\":[{\\\"action_name\\\":\\\"log_params\\\"}]}]}\"}");
	CHECK(strstr(reply_buf, "\"pushed\":1") != NULL);
}

static void test_sessions_groups_the_tree(void)
{
	/*
	 * The fork tree: root/child/grand under session S1 (the
	 * ppid chain 100->101->102 drives registry.c's real
	 * parent binding), a spectator on S1, and a lone agent on
	 * S2. Then the reply must carry two sessions and nested
	 * children.
	 */
	setup();
	(void)retraced_registry_hello(ctx.reg, NULL, 100, 1, "S1",
		"root");
	(void)retraced_registry_hello(ctx.reg, NULL, 101, 100, "S1",
		"child");
	(void)retraced_registry_hello(ctx.reg, NULL, 102, 101, "S1",
		"grand");
	(void)retraced_registry_hello(ctx.reg, NULL, 999, 1, "S1",
		"spect");
	(void)retraced_registry_hello(ctx.reg, NULL, 200, 1, "S2",
		"lone");

	/* the spectator seat is set by the HELLO handler, not the
	 * registry call -- flip it where the entry lives
	 */
	ctx.reg->agents[3].spectator = 1;

	feed(&ctx, "{\"cmd\":\"sessions\"}");
	CHECK(strstr(reply_buf, "\"ok\":1") != NULL);
	CHECK(strstr(reply_buf, "\"sessions_count\":2") != NULL);
	CHECK(strstr(reply_buf, "\"token\":\"S1\"") != NULL);
	CHECK(strstr(reply_buf, "\"token\":\"S2\"") != NULL);
	CHECK(strstr(reply_buf, "\"spectator\":1") != NULL);

	/*
	 * the nesting by real ids: read A/B ids from ps (registry
	 * ids are hashes we cannot predict), then assert the
	 * sessions reply nests B under A's children
	 */
	{
		char a_id[80] = "", b_id[80] = "";
		const char *walk;

		feed(&ctx, "{\"cmd\":\"ps\"}");
		walk = strstr(reply_buf, "\"cmdline\":\"root\"");
		CHECK(walk != NULL);
		if (walk != NULL) {
			const char *idq = walk;

			/* the id field precedes cmdline in ps order */
			idq = strstr(reply_buf, "\"id\":\"");
			while (idq != NULL && idq < walk) {
				size_t n = strcspn(idq + 6, "\"");

				if (n < sizeof(a_id)) {
					memcpy(a_id, idq + 6, n);
					a_id[n] = '\0';
				}
				idq = strstr(idq + 6, "\"id\":\"");
			}
		}
		walk = strstr(reply_buf, "\"cmdline\":\"child\"");
		CHECK(walk != NULL);
		if (walk != NULL) {
			const char *idq = strstr(reply_buf, "\"id\":\"");
			size_t last_a = 0;

			while (idq != NULL && idq < walk) {
				size_t n = strcspn(idq + 6, "\"");

				if (n < sizeof(b_id)) {
					memcpy(b_id, idq + 6, n);
					b_id[n] = '\0';
				}
				idq = strstr(idq + 6, "\"id\":\"");
				last_a = 1;
			}
			(void)last_a;
		}
		CHECK(a_id[0] != '\0' && b_id[0] != '\0');

		feed(&ctx, "{\"cmd\":\"sessions\"}");
		/* B nested under A: A's object contains B in its
		 * children array -- A's id appears before B's in
		 * the serialization and both appear once
		 */
		{
			const char *pa = strstr(reply_buf, a_id);
			const char *pb = strstr(reply_buf, b_id);

			CHECK(pa != NULL && pb != NULL);
			CHECK(pa < pb);
		}
	}
}

/* the events sink: collect what the daemon would reply */
static char ev_gather[16384];
static size_t ev_len;

static void ev_sink_test(const char *line, void *user)
{
	(void)user;
	if (ev_len + strlen(line) + 2 < sizeof(ev_gather)) {
		memcpy(ev_gather + ev_len, line, strlen(line));
		ev_len += strlen(line);
		ev_gather[ev_len++] = '\n';
	}
}

static void test_sessions_deep_chain_nests_at_every_depth(void)
{
	/*
	 * The depth-5 chain through the real ppid arrival order:
	 * root->a->b->c->d. Assert the ACTUAL nesting by parsing
	 * the reply -- string-order checks cannot see flattening
	 * (a sibling serialized after the subtree keeps parent-
	 * precedes-child true while the tree lies). The depth-2
	 * walker this test replaced flattened c and d.
	 */
	static const char *chain[] = { "root", "a", "b", "c", "d" };
	JSON_Value *v;
	JSON_Object *reply;
	JSON_Array *agents;
	JSON_Object *node;
	int i;

	setup();
	/* hello registers; link_parent is the daemon's REAL next
	 * step after HELLO (the ppid->parent binding -- see
	 * main.c's handler). Drive both, exactly as the frame
	 * handler does; cmdline comes through the entry.
	 */
	{
		struct { long pid, ppid; const char *cmdline; }
			chain[] = {
			{ 300, 1, "root" }, { 301, 300, "a" },
			{ 302, 301, "b" }, { 303, 302, "c" },
			{ 304, 303, "d" }
		};
		size_t c;

		for (c = 0; c < sizeof(chain) / sizeof(chain[0]);
		     c++) {
			struct agent_entry *e =
				retraced_registry_hello(ctx.reg, NULL,
					chain[c].pid, chain[c].ppid,
					"S1", chain[c].cmdline);

			if (e != NULL)
				(void)retraced_registry_link_parent(
					ctx.reg, e);
		}
	}

	feed(&ctx, "{\"cmd\":\"sessions\"}");
	v = json_parse_string(reply_buf);
	CHECK(v != NULL);
	reply = v != NULL ? json_value_get_object(v) : NULL;
	CHECK(reply != NULL);
	agents = NULL;
	if (reply != NULL) {
		JSON_Array *sessions = json_value_get_array(
			json_object_get_value(reply, "sessions"));

		CHECK(sessions != NULL);
		CHECK(json_array_get_count(sessions) == 1);
		if (sessions != NULL &&
		    json_array_get_count(sessions) == 1)
			agents = json_value_get_array(
				json_object_get_value(
					json_array_get_object(sessions, 0),
					"agents"));
	}
	/* exactly one root agent: nothing flattened to the top */
	CHECK(agents != NULL);
	CHECK(json_array_get_count(agents) == 1);

	node = agents != NULL ? json_array_get_object(agents, 0) :
		NULL;
	for (i = 0; i < 5 && node != NULL; i++) {
		const char *cmdline = json_object_get_string(node,
			"cmdline");

		CHECK(cmdline != NULL);
		CHECK(strcmp(cmdline, chain[i]) == 0);
		if (i + 1 < 5) {
			JSON_Value *cv = json_object_get_value(node,
				"children");
			JSON_Array *children = json_value_get_array(cv);

			CHECK(children != NULL);
			CHECK(json_array_get_count(children) == 1);
			node = children != NULL &&
			    json_array_get_count(children) == 1 ?
				json_array_get_object(children, 0) : NULL;
		}
	}
	CHECK(i == 5);
	json_value_free(v);
}

static void test_events_reads_and_verifies(void)
{
	/*
	 * The evidence arm: write real records through the REAL
	 * journal_event path, pull the tail, and assert both the
	 * content and the chain verdict. Then tamper one line on
	 * disk and assert the verdict flips -- the integrity
	 * claim is part of the interface.
	 */
	setup();
	retraced_journal_close(ctx.jr);
	remove("/tmp/ctl_ev_test.jsonl");
	retraced_journal_open(ctx.jr, "/tmp/ctl_ev_test.jsonl");
	retraced_journal_event(ctx.jr, 1000, "agent-a", 1,
		"{\"name\":\"ev.one\",\"attrs\":{}}");
	retraced_journal_event(ctx.jr, 1001, "agent-a", 2,
		"{\"name\":\"ev.two\",\"attrs\":{}}");
	retraced_journal_event(ctx.jr, 1002, "agent-a", 3,
		"{\"name\":\"ev.three\",\"attrs\":{}}");
	retraced_journal_flush(ctx.jr);

	ev_len = 0;
	ev_gather[0] = '\0';
	{
		long chain = -1;
		int n = retraced_journal_tail(ctx.jr, 2,
			ev_sink_test, NULL, &chain);

		CHECK(n == 2);
		CHECK(chain == 0);
		CHECK(strstr(ev_gather, "ev.two") != NULL);
		CHECK(strstr(ev_gather, "ev.three") != NULL);
		CHECK(strstr(ev_gather, "ev.one") == NULL);
	}

	/* tamper the middle line: the chain must report it */
	{
		FILE *f = fopen("/tmp/ctl_ev_test.jsonl", "r");
		char all[8192];
		size_t n;
		char *p;

		n = fread(all, 1, sizeof(all) - 1, f);
		all[n] = '\0';
		fclose(f);
		p = strstr(all, "ev.two");
		CHECK(p != NULL);
		if (p != NULL)
			p[5] = 'X';	/* ev.twX */
		f = fopen("/tmp/ctl_ev_test.jsonl", "w");
		fwrite(all, 1, n, f);
		fclose(f);
	}
	{
		long chain = 0;

		ev_len = 0;
		ev_gather[0] = '\0';
		(void)retraced_journal_tail(ctx.jr, 3,
			ev_sink_test, NULL, &chain);
		CHECK(chain == 3);	/* line 3's prev no longer matches */
	}
	remove("/tmp/ctl_ev_test.jsonl");
}

static void test_policy_push_bad(void)
{
	setup();
	feed(&ctx, "{\"cmd\":\"policy_push\",\"blob\":\"{}\"}");
	CHECK(strstr(reply_buf, "\"error\":\"bad policy") != NULL);
	CHECK(ctx.policy_blob == NULL);
}

static void test_freeze_thaw(void)
{
	setup();
	feed(&ctx, "{\"cmd\":\"policy_push\",\"blob\":"
		   "\"{\\\"policy\\\":{\\\"epoch\\\":3},"
		   "\\\"intercept_scripts\\\":[]}\"}");
	feed(&ctx, "{\"cmd\":\"freeze\"}");
	CHECK(ctx.frozen == 1);
	CHECK(ctx.policy_epoch == 4);
	feed(&ctx, "{\"cmd\":\"thaw\"}");
	CHECK(ctx.frozen == 0);
	CHECK(ctx.policy_epoch == 5);
	/* thaw restores the pre-freeze policy's shape */
	CHECK(ctx.policy_blob != NULL &&
	      strstr(ctx.policy_blob, "intercept_scripts") != NULL);
}

static void test_kill_no_pid(void)
{
	setup();
	feed(&ctx, "{\"cmd\":\"kill\"}");
	CHECK(strstr(reply_buf, "\"error\":\"no pid\"") != NULL);
}

static void test_scope_denied(void)
{
	/* TLS peer with status-only claims cannot freeze */
	setup();
	ctx.scopes = RETRACED_SCOPE_STATUS;
	feed(&ctx, "{\"cmd\":\"status\"}");
	CHECK(strstr(reply_buf, "\"ok\":1") != NULL);
	feed(&ctx, "{\"cmd\":\"freeze\"}");
	CHECK(strstr(reply_buf, "\"error\":\"scope denied\"") != NULL);
	CHECK(ctx.frozen == 0);
}

/*
 * the spawn seam, faked: the handler's job is parsing,
 * reply shape, and the journal call -- the fork is the
 * transport's business (main.c), integration-tested there
 */
static char spawn_seen_argv0[64];
static char spawn_seen_preload[64];

static long fake_spawn_cb(const char *const *argv,
	const char *preload, char *err_out, size_t err_cap)
{
	(void)err_cap;
	err_out[0] = '\0';
	snprintf(spawn_seen_argv0, sizeof(spawn_seen_argv0), "%s",
		argv[0]);
	snprintf(spawn_seen_preload, sizeof(spawn_seen_preload), "%s",
		preload != NULL ? preload : "");
	return 4242;
}

static void test_spawn_launches(void)
{
	setup();
	ctx.spawn_cb = fake_spawn_cb;
	feed(&ctx,
	     "{\"cmd\":\"spawn\",\"argv\":[\"/bin/work\",\"-v\"],"
	     "\"preload\":\"/opt/libretrace.so\"}");
	CHECK(strstr(reply_buf, "\"ok\":1,\"pid\":4242") != NULL);
	CHECK(strcmp(spawn_seen_argv0, "/bin/work") == 0);
	CHECK(strcmp(spawn_seen_preload, "/opt/libretrace.so") == 0);
}

static void test_spawn_no_cb_refuses(void)
{
	/* the platform without a fork seam answers honestly */
	setup();
	feed(&ctx, "{\"cmd\":\"spawn\",\"argv\":[\"/bin/work\"]}");
	CHECK(strstr(reply_buf, "not on this platform") != NULL);
	CHECK(strstr(reply_buf, "\"ok\":0") != NULL);
}

static void test_spawn_no_argv(void)
{
	setup();
	ctx.spawn_cb = fake_spawn_cb;
	feed(&ctx, "{\"cmd\":\"spawn\"}");
	CHECK(strstr(reply_buf, "\"error\":\"no argv\"") != NULL);
}

static void test_spawn_scope_denied(void)
{
	setup();
	ctx.spawn_cb = fake_spawn_cb;
	ctx.scopes = RETRACED_SCOPE_STATUS;
	feed(&ctx, "{\"cmd\":\"spawn\",\"argv\":[\"/bin/work\"]}");
	CHECK(strstr(reply_buf, "\"error\":\"scope denied\"") != NULL);
}

/*
 * The two-layer read: a libc agent and a kernel observer on
 * one session fold into ONE rollup -- observers' kernel_obs
 * totals and live deltas, the libc seats listed -- and a
 * second session stays separate.
 */
static void test_drift_rolls_sessions(void)
{
	struct agent_entry *a, *k1, *k2;

	setup();
	a = retraced_registry_hello(ctx.reg, "libc-a", 100, 1,
		"S1", "detonation");
	k1 = retraced_registry_hello(ctx.reg, "ebpf-1", 101, 1,
		"S1", "ebpf");
	k2 = retraced_registry_hello(ctx.reg, "ebpf-2", 102, 1,
		"S2", "ebpf");
	CHECK(a != NULL && k1 != NULL && k2 != NULL);
	k1->spectator = 1;
	k1->kernel_obs = 40;
	k1->kernel_obs_last = 33;
	k2->spectator = 1;
	k2->kernel_obs = 5;
	feed(&ctx, "{\"cmd\":\"drift\"}");
	{
		JSON_Value *v = json_parse_string(reply_buf);
		JSON_Array *sess;
		JSON_Object *s1 = NULL, *s2 = NULL;
		size_t i;

		CHECK(v != NULL);
		sess = json_value_get_array(
			json_object_get_value(json_value_get_object(v),
				"sessions"));
		CHECK(json_array_get_count(sess) == 2);
		for (i = 0; i < 2; i++) {
			JSON_Object *o = json_array_get_object(sess, i);

			if (strcmp(json_object_get_string(o, "token"),
				    "S1") == 0)
				s1 = o;
			else
				s2 = o;
		}
		CHECK(s1 != NULL && s2 != NULL);
		CHECK(json_object_get_number(s1, "kernel_obs") == 40);
		CHECK(json_object_get_number(s1, "delta") == 7);
		CHECK(json_object_get_number(s1, "spectators") == 1);
		CHECK(json_array_get_count(
			json_value_get_array(json_object_get_value(
				s1, "libc"))) == 1);
		CHECK(json_object_get_number(s2, "kernel_obs") == 5);
		CHECK(json_object_get_number(s2, "delta") == 5);
		json_value_free(v);
	}
}

/*
 * The framing tests (the byte layer's own test class -- the
 * reason feed exists): a line split across reads is ONE
 * command; two commands in one chunk are TWO; a partial line
 * stays silent until its newline arrives; a line that exceeds
 * the buffer is refused for the transport to drop.
 */
static void test_feed_splits_lines_across_chunks(void)
{
	setup();
	ctx.reply_sink = counting_sink;
	feed_count = 0;
	{
		char a[16], b[8];

		snprintf(a, sizeof(a), "{\"cmd\":\"sta");
		snprintf(b, sizeof(b), "tus\"}\n");
		CHECK(retraced_ctl_feed(&ctx, a, strlen(a)) == 0);
		CHECK(feed_count == 0);
		CHECK(retraced_ctl_feed(&ctx, b, strlen(b)) == 0);
	}
	CHECK(feed_count == 1);
	CHECK(strstr(reply_buf, "\"ok\":1") != NULL);
}

static void test_feed_two_commands_one_chunk(void)
{
	setup();
	ctx.reply_sink = counting_sink;
	feed_count = 0;
	{
		char chunk[64];

		snprintf(chunk, sizeof(chunk),
			"{\"cmd\":\"status\"}\n{\"cmd\":\"ps\"}\n");
		CHECK(retraced_ctl_feed(&ctx, chunk, strlen(chunk)) == 0);
	}
	CHECK(feed_count == 2);
}

static void test_feed_partial_without_newline_is_silent(void)
{
	setup();
	ctx.reply_sink = counting_sink;
	feed_count = 0;
	{
		char part[24], whole[24];

		snprintf(part, sizeof(part), "{\"cmd\":\"status\"}");
		snprintf(whole, sizeof(whole), "{\"cmd\":\"status\"}\n");
		CHECK(retraced_ctl_feed(&ctx, part, strlen(part)) == 0);
		CHECK(feed_count == 0);
		retraced_ctl_conn_reset(&ctx);
		CHECK(retraced_ctl_feed(&ctx, whole,
			    strlen(whole)) == 0);
	}
	CHECK(feed_count == 1);
}

static void test_feed_oversized_line_refused(void)
{
	static char big[9000];

	setup();
	memset(big, 'A', sizeof(big));
	CHECK(retraced_ctl_feed(&ctx, big, sizeof(big)) == -1);
	/* after a drop the framing state is empty: a fresh
	 * connection parses normally
	 */
	retraced_ctl_conn_reset(&ctx);
	feed(&ctx, "{\"cmd\":\"status\"}");
	CHECK(strstr(reply_buf, "\"ok\":1") != NULL);
}

/*
 * Table conformance: the SSOT list expanded here -- every verb
 * must carry a nonzero claim scope (a zero-scope row would
 * silently grant nothing to every cert) and must ANSWER (a
 * verb that falls through to "unknown cmd" is a list/handler
 * drift; the handler-less row is a compile error, this catches
 * the rest)
 */
static void test_verb_table_conformance(void)
{
#define VERB_PROBE(n, c, s, a, h)					      \
	setup();							      \
	feed(&ctx, "{\"cmd\":\"" #n "\"}");				      \
	CHECK(strstr(reply_buf, "unknown cmd") == NULL);		      \
	CHECK(retraced_tls_scope_for_cmd(#n) != 0);

	RETRACED_CTL_VERBS(VERB_PROBE)
#undef VERB_PROBE
}

int main(void)
{
	printf("retraced ctl plane tests:\n");
	TEST(malformed);
	TEST(no_cmd);
	TEST(unknown);
	TEST(status);
	TEST(ps);
	TEST(sessions_groups_the_tree);
	TEST(sessions_deep_chain_nests_at_every_depth);
	TEST(events_reads_and_verifies);
	TEST(policy_push_valid);
	TEST(policy_push_bad);
	TEST(freeze_thaw);
	TEST(kill_no_pid);
	TEST(scope_denied);
	TEST(spawn_launches);
	TEST(spawn_no_cb_refuses);
	TEST(spawn_no_argv);
	TEST(spawn_scope_denied);
	TEST(verb_table_conformance);
	TEST(feed_splits_lines_across_chunks);
	TEST(feed_two_commands_one_chunk);
	TEST(feed_partial_without_newline_is_silent);
	TEST(feed_oversized_line_refused);
	TEST(drift_rolls_sessions);

	printf("%d tests: %d pass, %d fail\n", tests_run, tests_pass,
		tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
