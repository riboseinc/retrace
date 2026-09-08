/*
 * Copyright (c) 2017, [Ribose Inc](https://ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Evidence redaction (TODO.impl/01): the token semantics and
 * the two contract sides -- zero-delta when unarmed, exact
 * token replacement when armed.
 */

#include <stdio.h>
#include <string.h>

#include "redact.h"

static int tests_run;
static int tests_fail;

#define TEST(name) do { \
	tests_run++; \
	printf("  TEST %s ... ", #name); \
	test_##name(); \
	tests_fail += g_failed; \
	printf("%s\n", g_failed ? "FAIL" : "OK"); \
	g_failed = 0; \
} while (0)

static int g_failed;

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("\n    CHECK failed [%s:%d] %s\n", __FILE__, \
			__LINE__, #cond); \
		g_failed = 1; \
		return; \
	} \
} while (0)

static void set1(const char *a)
{
	const char *pats[1] = { a };

	retrace_redact_set(pats, 1);
}

static void apply_expect(const char *in, const char *want)
{
	char buf[256];

	snprintf(buf, sizeof(buf), "%s", in);
	retrace_redact_apply(buf);
	if (strcmp(buf, want) != 0) {
		printf("\n    got '%s' want '%s'", buf, want);
		g_failed = 1;
	}
}

static void test_unarmed_zero_delta(void)
{
	retrace_redact_set(NULL, 0);
	CHECK(retrace_redact_active() == 0);
	apply_expect("SECRET_TOKEN=hunter2 unchanged",
		"SECRET_TOKEN=hunter2 unchanged");
}

static void test_whole_token_glob(void)
{
	set1("*_TOKEN");
	CHECK(retrace_redact_active() == 1);
	/* the token extends to delimiters, not past them */
	apply_expect("env SECRET_TOKEN here", "env *** here");
	/* '=' is INSIDE tokens: a key=value pair is one unit, and
	 * "*_TOKEN" (anchored end) does not span the value --
	 * "*_TOKEN*" takes the pair whole
	 */
	apply_expect("SECRET_TOKEN=hunter2",
		"SECRET_TOKEN=hunter2");
	set1("*_TOKEN*");
	apply_expect("SECRET_TOKEN=hunter2", "***");
	/* classic glob: a trailing '*' unanchors the end, so the
	 * longer token also matches; the END-ANCHORED form leaves
	 * it alone
	 */
	apply_expect("MY_SECRET_TOKEN2", "***");
	set1("*_TOKEN");
	apply_expect("MY_SECRET_TOKEN2", "MY_SECRET_TOKEN2");
}

static void test_prefix_glob_covers_values(void)
{
	set1("token=x96*");
	apply_expect("/tmp/token=x96_file", "/tmp/***");
	apply_expect("token=x96", "***");
	apply_expect("atoken=x96z", "atoken=x96z");
	set1("AWS_*");
	/* the pair-shaped token starts at the key: pattern the
	 * key's prefix
	 */
	apply_expect("?AWS_SECRET_ACCESS_KEY=aksk&r=1",
		"?***&r=1");
	apply_expect("key=AWS_SECRET_ACCESS_KEY&x=1",
		"key=AWS_SECRET_ACCESS_KEY&x=1");
}

static void test_literal_and_multiple(void)
{
	set1("hunter2");
	apply_expect("got hunter2 and hunter2", "got *** and ***");
	{
		const char *pats[2] = { "*_TOKEN*", "hunter2" };

		retrace_redact_set(pats, 2);
	}
	apply_expect("SECRET_TOKEN=hunter2", "***");
}

static void test_delimiters_shape_tokens(void)
{
	set1("*TOKEN*");
	/* '/' '?' '&' delimit; '=' does not: q=TOKEN96 is one
	 * token and goes whole
	 */
	apply_expect("/a/b?q=TOKEN96&r=1", "/a/b?***&r=1");
	/* the END-ANCHORED form matches only a token that ENDS
	 * with the shape
	 */
	set1("*TOKEN");
	apply_expect("XTOKENY", "XTOKENY");
	/* q=XTOKEN is one pair-shaped token and goes whole */
	apply_expect("q=XTOKEN", "***");
}

static void test_csv_form(void)
{
	retrace_redact_set_csv("*_KEY*,hunter2");
	CHECK(retrace_redact_active() == 1);
	apply_expect("API_KEY=k1 hunter2", "*** ***");
}

static void test_budget_bounds_replacement(void)
{
	char in[512];

	retrace_redact_set_csv("x*");
	memset(in, 'x', sizeof(in) - 1);
	in[sizeof(in) - 1] = '\0';
	/* one giant token: one replacement, then bounded */
	retrace_redact_apply(in);
	/* must terminate, content prefix replaced */
	CHECK(in[0] == '*');
}

int main(void)
{
	printf("evidence redaction tests:\n");
	TEST(unarmed_zero_delta);
	TEST(whole_token_glob);
	TEST(prefix_glob_covers_values);
	TEST(literal_and_multiple);
	TEST(delimiters_shape_tokens);
	TEST(csv_form);
	TEST(budget_bounds_replacement);
	printf("%d tests: %d fail\n", tests_run, tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
