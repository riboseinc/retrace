/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for retrace_as_call_real_dispatch (TODO 28):
 * the call_real action routes scripted calls through a C-level
 * dispatch whose switch must cover the prototype arities. The
 * ntdll file API reaches 11 (NtCreateFile) and 12
 * (NtQueryDirectoryFile) -- before the extension, params > 6
 * hit `default: -1` and the REAL FUNCTION WAS NEVER CALLED
 * (CI: hooked NtCreateFile broke every fopen).
 *
 * A 12-arg sum function proves every arity 7..12 round-trips;
 * arity 13 must be refused with -1.
 */

#include <stdio.h>
#include <string.h>

#include "arch_spec.h"
#include "engine.h"
#include "real_impls.h"

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

/*
 * Per-arity sum functions: calling a WIDER function through a
 * narrower signature leaves the missing args as caller-stack
 * garbage -- each arity must round-trip through its OWN shape.
 */
#define SUM_FN(n) \
static long sum##n(long a1, long a2, long a3, long a4, long a5, \
	long a6, long a7, long a8, long a9, long a10, long a11, \
	long a12) \
{ \
	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; \
	(void)a6; (void)a7; (void)a8; (void)a9; (void)a10; \
	(void)a11; (void)a12; \
	switch (n) { \
	case 7: return a1 + a2 + a3 + a4 + a5 + a6 + a7; \
	case 8: return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8; \
	case 9: return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9; \
	case 10: return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + \
			a9 + a10; \
	case 11: return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + \
			a9 + a10 + a11; \
	default: return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + \
			a9 + a10 + a11 + a12; \
	} \
}
SUM_FN(7)
SUM_FN(8)
SUM_FN(9)
SUM_FN(10)
SUM_FN(11)
SUM_FN(12)

static const void *g_sum_fns[] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	sum7, sum8, sum9, sum10, sum11, sum12
};

static void fill_params(struct FuncParam *params, int n)
{
	int i;

	memset(params, 0, sizeof(*params) * (size_t)n);
	for (i = 0; i < n; i++)
		params[i].val = (intptr_t)(i + 1);
}

static void test_dispatch_7_to_12(void)
{
	struct FuncParam params[16];
	int n;

	for (n = 7; n <= 12; n++) {
		intptr_t expect;
		intptr_t got;

		fill_params(params, n);
		expect = (intptr_t)n * (n + 1) / 2; /* sum 1..n */
		got = retrace_as_call_real_dispatch(g_sum_fns[n],
			params, n);
		CHECK(got == expect);
	}
}

static void test_dispatch_refuses_13(void)
{
	struct FuncParam params[16];

	fill_params(params, 13);
	CHECK(retrace_as_call_real_dispatch(sum12, params, 13) == -1);
}

int main(void)
{
	printf("as_call_real_dispatch tests:\n");

	TEST(dispatch_7_to_12);
	TEST(dispatch_refuses_13);

	printf("Pass: %d, Fail: %d (of %d)\n", tests_pass, tests_fail,
		tests_run);
	return tests_fail == 0 ? 0 : 1;
}
