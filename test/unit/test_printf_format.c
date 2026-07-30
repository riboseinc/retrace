/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the parse_printf_format shim (src/core/printf_compat.h).
 *
 * On glibc/Darwin the system parse_printf_format is used; on musl
 * (Alpine, OHOS) our shim is compiled in via printf_compat.h. This
 * test forces the shim to be compiled in by including the header
 * after undefining HAVE_PRINTF_H, so we always test our own code
 * path regardless of the build host.
 *
 * Issue #481: extend unit test layer to cover core utilities.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Force the shim. The real system header (when present) provides
 * parse_printf_format with the same signature and semantics, so a
 * host that has printf.h will see both definitions and conflict.
 * Avoid that by renaming our shim via macro.
 */
#define parse_printf_format retrace_parse_printf_format_test
#undef HAVE_CONFIG_H
#undef HAVE_PRINTF_H

#include "printf_compat.h"

#undef parse_printf_format

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

/* retrace_parse_printf_format_test is our local rename of the shim. */
static int (*fmt_parse)(const char *, int, int *) = retrace_parse_printf_format_test;

static void test_no_args(void)
{
	assert(fmt_parse("hello world", 0, NULL) == 0);
}

static void test_literal_percent(void)
{
	assert(fmt_parse("100%% done", 0, NULL) == 0);
}

static void test_single_int(void)
{
	int types[1];
	int n = fmt_parse("%d", 1, types);

	assert(n == 1);
	assert(types[0] == PA_INT);
}

static void test_multiple_ints(void)
{
	int types[3];
	int n = fmt_parse("%d %i %u", 3, types);

	assert(n == 3);
	assert(types[0] == PA_INT);
	assert(types[1] == PA_INT);
	assert(types[2] == PA_INT);
}

static void test_string(void)
{
	int types[1];

	assert(fmt_parse("%s", 1, types) == 1);
	assert(types[0] == PA_STRING);
}

static void test_char(void)
{
	int types[1];

	assert(fmt_parse("%c", 1, types) == 1);
	assert(types[0] == PA_CHAR);
}

static void test_pointer(void)
{
	int types[1];

	assert(fmt_parse("%p", 1, types) == 1);
	assert(types[0] == PA_POINTER);
}

static void test_long_modifier(void)
{
	int types[1];

	assert(fmt_parse("%ld", 1, types) == 1);
	assert((types[0] & PA_FLAG_LONG) != 0);
	assert((types[0] & ~PA_FLAG_MASK) == PA_INT);
}

static void test_long_long_modifier(void)
{
	int types[1];

	assert(fmt_parse("%lld", 1, types) == 1);
	assert((types[0] & PA_FLAG_LONG_LONG) != 0);
}

static void test_short_modifier(void)
{
	int types[1];

	assert(fmt_parse("%hd", 1, types) == 1);
	assert((types[0] & PA_FLAG_SHORT) != 0);
}

static void test_two_pass_count(void)
{
	int n = fmt_parse("%d %s %d", 0, NULL);
	int *types = (int *)malloc(sizeof(int) * n);

	assert(n == 3);
	fmt_parse("%d %s %d", n, types);
	assert(types[0] == PA_INT);
	assert(types[1] == PA_STRING);
	assert(types[2] == PA_INT);
	free(types);
}

static void test_width_star(void)
{
	int types[2];

	assert(fmt_parse("%*d", 2, types) == 2);
	assert(types[0] == PA_INT);
	assert(types[1] == PA_INT);
}

static void test_precision_star(void)
{
	int types[2];

	assert(fmt_parse("%.*d", 2, types) == 2);
	assert(types[0] == PA_INT);
	assert(types[1] == PA_INT);
}

static void test_flags_ignored(void)
{
	int types[1];

	assert(fmt_parse("%+0#- d", 1, types) == 1);
	assert(types[0] == PA_INT);
}

static void test_fp_bail(void)
{
	/* Shim returns 0 when any FP conversion is detected. */
	assert(fmt_parse("%f", 0, NULL) == 0);
	assert(fmt_parse("%.2f", 0, NULL) == 0);
	assert(fmt_parse("%e %g %a", 0, NULL) == 0);
}

static void test_fp_bail_mixed(void)
{
	/* Even one FP in a mixed format triggers the bail. */
	assert(fmt_parse("%d %f %s", 0, NULL) == 0);
}

static void test_malformed_truncated(void)
{
	/* Unterminated conversion: stop gracefully. */
	assert(fmt_parse("hello %", 0, NULL) == 0);
}

static void test_n_writes_pointer(void)
{
	int types[1];

	assert(fmt_parse("%n", 1, types) == 1);
	assert((types[0] & ~PA_FLAG_MASK) == PA_POINTER);
}

int main(void)
{
	printf("parse_printf_format shim:\n");
	TEST(no_args);
	TEST(literal_percent);
	TEST(single_int);
	TEST(multiple_ints);
	TEST(string);
	TEST(char);
	TEST(pointer);
	TEST(long_modifier);
	TEST(long_long_modifier);
	TEST(short_modifier);
	TEST(two_pass_count);
	TEST(width_star);
	TEST(precision_star);
	TEST(flags_ignored);
	TEST(fp_bail);
	TEST(fp_bail_mixed);
	TEST(malformed_truncated);
	TEST(n_writes_pointer);

	printf("\n%d run, %d passed, %d failed\n",
		tests_run, tests_pass, tests_fail);
	return tests_fail ? 1 : 0;
}
