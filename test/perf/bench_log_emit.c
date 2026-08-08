/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Micro-benchmark: log emit cost per entry (TODO.complete/34 P0).
 *
 * Measures the per-call cost of log_info (the most common log
 * path -- every action uses it). Includes formatting + I/O.
 *
 * Output goes to stderr by default; stdout is reserved for the
 * bench result line.
 */

#include "bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "real_impls.h"
#include "logger.h"

static int g_counter;

static void bench_log_info_fixed(void *ctx)
{
	log_info("bench entry");
	(void)ctx;
}

static void bench_log_info_format(void *ctx)
{
	log_info("bench entry %d with %s", g_counter++, "data");
	(void)ctx;
}

int main(void)
{
	struct bench_result r;

	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;
	retrace_real_impls.real_sprintf = sprintf;
	retrace_real_impls.real_vsnprintf = vsnprintf;

	printf("--- log_emit benchmark ---\n");

	g_counter = 0;
	if (bench_run("log_info_fixed", bench_log_info_fixed, NULL,
		10000, &r) == 0)
		bench_print("log_info_fixed", &r);

	g_counter = 0;
	if (bench_run("log_info_format", bench_log_info_format, NULL,
		10000, &r) == 0)
		bench_print("log_info_format", &r);

	return 0;
}
