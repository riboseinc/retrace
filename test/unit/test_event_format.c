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
 * The evidence pipeline's stack fast-path formatter: the common
 * event formats with zero allocations; anything needing
 * escaping or overflowing the buffer declines (-1) so the heap
 * path (jesc) handles it. These tests pin both sides of that
 * contract.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "agent.h"

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

static void test_plain(void)
{
	char buf[768];
	const char *kv[] = { "retrace.jail.path", "/etc/hosts" };
	int rc = retrace_agent_format_event_stack(buf, sizeof(buf),
		"boot.1.1", 7, "retrace.jail.denied", kv, 1);

	CHECK(rc == 0);
	CHECK(strstr(buf, "\"name\":\"retrace.jail.denied\"") != NULL);
	CHECK(strstr(buf, "\"seq\":7") != NULL);
	CHECK(strstr(buf, "\"retrace.jail.path\":\"/etc/hosts\"")
		!= NULL);
	CHECK(strstr(buf, "\"agent_id\":\"boot.1.1\"") != NULL);
	/* well-formed JSON object shape */
	CHECK(buf[0] == '{' && buf[strlen(buf) - 1] == '}');
}

static void test_no_attrs(void)
{
	char buf[768];

	CHECK(retrace_agent_format_event_stack(buf, sizeof(buf),
		"a", 1, "retrace.ping", NULL, 0) == 0);
	CHECK(strstr(buf, "\"attrs\":{}") != NULL);
}

static void test_quote_declines(void)
{
	char buf[768];
	const char *kv[] = { "k", "va\"lue" };

	CHECK(retrace_agent_format_event_stack(buf, sizeof(buf),
		"a", 1, "retrace.jail.denied", kv, 1) == -1);
}

static void test_backslash_declines(void)
{
	char buf[768];
	const char *kv[] = { "path", "C:\\evil" };

	CHECK(retrace_agent_format_event_stack(buf, sizeof(buf),
		"a", 1, "retrace.jail.denied", kv, 1) == -1);
}

static void test_oversize_declines(void)
{
	char small[64];

	CHECK(retrace_agent_format_event_stack(small, sizeof(small),
		"agent-id-longer-than-buffer", 1,
		"retrace.jail.denied", NULL, 0) == -1);
}

int main(void)
{
	printf("event stack formatter tests:\n");
	TEST(plain);
	TEST(no_attrs);
	TEST(quote_declines);
	TEST(backslash_declines);
	TEST(oversize_declines);

	printf("%d tests: %d pass, %d fail\n", tests_run, tests_pass,
		tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
