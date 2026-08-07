/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Micro-benchmark: caller_match address-kind cost
 * (TODO.complete/34 P0).
 *
 * Measures the time for retrace_caller_match_address() to
 * evaluate a single address match. The address matcher is a
 * single integer compare, so this serves as the baseline
 * ("minimum possible cost per match").
 *
 * Also measures retrace_caller_match_symbol() which calls
 * dladdr. Expected ~1000x slower because of dladdr's symbol
 * table walk.
 */

#include "bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "caller_match.h"
#include "real_impls.h"

static void bench_address_match(void *ctx)
{
	unsigned long long *expected = (unsigned long long *)ctx;
	int marker;

	(void)retrace_caller_match_address(&marker, *expected);
}

static void bench_address_mismatch(void *ctx)
{
	unsigned long long *expected = (unsigned long long *)ctx;
	int marker;

	(void)retrace_caller_match_address(&marker, *expected + 1);
}

static void bench_symbol_match(void *ctx)
{
	(void)retrace_caller_match_symbol((void *)bench_symbol_match,
		"bench_symbol_match");
	(void)ctx;
}

int main(void)
{
	struct bench_result r;
	unsigned long long expected;
	int marker;

	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;

	expected = (unsigned long long)(unsigned long)&marker;

	printf("--- caller_match benchmark ---\n");

	if (bench_run("address_match", bench_address_match, &expected,
		1000000, &r) == 0)
		bench_print("address_match", &r);

	if (bench_run("address_mismatch", bench_address_mismatch, &expected,
		1000000, &r) == 0)
		bench_print("address_mismatch", &r);

	if (bench_run("symbol_match_via_dladdr", bench_symbol_match, NULL,
		10000, &r) == 0)
		bench_print("symbol_match_via_dladdr", &r);

	return 0;
}
