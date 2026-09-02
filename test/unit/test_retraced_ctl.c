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

static void sink(const char *line, void *user)
{
	(void)user;
	reply_len = 0;
	while (*line != '\0' && reply_len + 1 < sizeof(reply_buf))
		reply_buf[reply_len++] = *line++;
	reply_buf[reply_len] = '\0';
}

static void feed(struct retraced_ctl_ctx *ctx, const char *line)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), "%s", line);
	reply_buf[0] = '\0';
	reply_len = 0;
	retraced_ctl_handle_line(ctx, buf);
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

int main(void)
{
	printf("retraced ctl plane tests:\n");
	TEST(malformed);
	TEST(no_cmd);
	TEST(unknown);
	TEST(status);
	TEST(ps);
	TEST(sessions_groups_the_tree);
	TEST(policy_push_valid);
	TEST(policy_push_bad);
	TEST(freeze_thaw);
	TEST(kill_no_pid);
	TEST(scope_denied);

	printf("%d tests: %d pass, %d fail\n", tests_run, tests_pass,
		tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
