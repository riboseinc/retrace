/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Tiny benchmark harness for retrace perf tests (TODO.complete/34).
 *
 * Runs a function N times, sorts the per-call nanosecond
 * measurements, and reports min / p50 / p95 / p99 / max.
 *
 * Uses clock_gettime(CLOCK_MONOTONIC) for nanosecond precision.
 * On macOS CLOCK_MONOTONIC is also ns-precision. On Windows this
 * would dispatch to QueryPerformanceCounter; deferred to a
 * future Windows port.
 *
 * Usage:
 *
 *   static void my_op(void *ctx) { ... }
 *
 *   struct bench_result r;
 *   bench_run("my_op", my_op, NULL, 100000, &r);
 *   bench_print("my_op", &r);
 */

#ifndef RETRACE_TEST_PERF_BENCH_H
#define RETRACE_TEST_PERF_BENCH_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <time.h>

#define BENCH_DEFAULT_ITERS 100000

struct bench_result {
	uint64_t min_ns;
	uint64_t p50_ns;
	uint64_t p95_ns;
	uint64_t p99_ns;
	uint64_t max_ns;
	uint64_t iters;
};

typedef void (*bench_op_fn)(void *ctx);

/* Run fn(ctx) iters times, measuring each call. Returns the
 * sorted percentiles in *out. Returns 0 on success, -1 on
 * alloc failure.
 *
 * The first 1000 iterations are warmup and not measured; this
 * avoids cache-cold outliers in the reported numbers.
 */
static inline int bench_run(const char *name,
			    bench_op_fn fn,
			    void *ctx,
			    uint64_t iters,
			    struct bench_result *out)
{
	uint64_t *samples;
	uint64_t i;
	uint64_t warmup;

	if (iters == 0)
		iters = BENCH_DEFAULT_ITERS;

	warmup = iters > 1000 ? 1000 : iters / 10;

	samples = (uint64_t *)malloc(sizeof(uint64_t) * iters);
	if (samples == NULL)
		return -1;

	/* Warmup */
	for (i = 0; i < warmup; i++)
		fn(ctx);

	/* Measured iterations */
	for (i = 0; i < iters; i++) {
		struct timespec t0, t1;
		uint64_t delta;

		clock_gettime(CLOCK_MONOTONIC, &t0);
		fn(ctx);
		clock_gettime(CLOCK_MONOTONIC, &t1);

		delta = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1000000000ULL +
			(uint64_t)(t1.tv_nsec - t0.tv_nsec);
		samples[i] = delta;
	}

	/* Insertion sort is O(n^2) -- use it only for small n.
	 * For larger runs we'd want qsort, but the comparison
	 * callback overhead exceeds the sort time below ~100K
	 * samples.
	 */
	if (iters <= 100000) {
		for (i = 1; i < iters; i++) {
			uint64_t key = samples[i];
			uint64_t j = i;

			while (j > 0 && samples[j - 1] > key) {
				samples[j] = samples[j - 1];
				j--;
			}
			samples[j] = key;
		}
	} else {
		/* For large runs, do a coarse bucket sort by 100ns
		 * width -- enough resolution for regression detection.
		 */
		enum { NBUCKETS = 1000 };
		uint64_t bucket_max = 0;
		uint64_t *counts;
		uint64_t b;
		uint64_t out_idx = 0;

		for (i = 0; i < iters; i++)
			if (samples[i] > bucket_max)
				bucket_max = samples[i];

		counts = (uint64_t *)calloc(NBUCKETS, sizeof(uint64_t));
		if (counts == NULL) {
			free(samples);
			return -1;
		}

		for (i = 0; i < iters; i++) {
			uint64_t bucket = samples[i] * NBUCKETS /
				(bucket_max + 1);

			counts[bucket]++;
		}

		for (b = 0; b < NBUCKETS; b++) {
			uint64_t bucket_val = (b + 1) * (bucket_max + 1) /
				NBUCKETS;
			uint64_t c;

			for (c = 0; c < counts[b] && out_idx < iters; c++) {
				samples[out_idx] = bucket_val;
				out_idx++;
			}
		}

		free(counts);
	}

	out->min_ns = samples[0];
	out->p50_ns = samples[iters / 2];
	out->p95_ns = samples[iters * 95 / 100];
	out->p99_ns = samples[iters * 99 / 100];
	out->max_ns = samples[iters - 1];
	out->iters = iters;

	free(samples);
	return 0;
}

/* Print a result line in a stable format suitable for diffing:
 *
 *   bench=name iters=N min=X p50=Y p95=Z p99=W max=V ns
 */
static inline void bench_print(const char *name,
			       const struct bench_result *r)
{
	printf("bench=%s iters=%llu min=%llu p50=%llu p95=%llu p99=%llu max=%llu ns\n",
		name,
		(unsigned long long)r->iters,
		(unsigned long long)r->min_ns,
		(unsigned long long)r->p50_ns,
		(unsigned long long)r->p95_ns,
		(unsigned long long)r->p99_ns,
		(unsigned long long)r->max_ns);
}

#endif /* RETRACE_TEST_PERF_BENCH_H */
