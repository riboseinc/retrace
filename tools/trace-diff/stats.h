/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_DIFF_STATS_H_
#define RETRACE_DIFF_STATS_H_

#include <stddef.h>
#include <stdint.h>

/*
 * Statistical significance for the differential trace analysis
 * (TODO.complete/27 P2).
 *
 * Owns the per-function z-score math: given N baseline call
 * counts and one test count, decide whether the test deviates
 * from the baseline distribution by more than a threshold of
 * standard deviations. Loading traces, iterating functions and
 * printing live in diff.c.
 */

struct diff_stats {
	double mean;         /* baseline mean */
	double stddev;       /* baseline stddev (population) */
	double z;            /* (test - mean) / stddev; 0 when no variance */

	/* Baselines had zero variance (all equal). The decision rule
	 * degrades to "any deviation from the constant is notable"
	 * because a real z is undefined (divide by zero).
	 */
	int no_variance;

	/* The decision: test deviates by more than `threshold`
	 * stddevs. With no_variance, true for any deviation from
	 * the constant mean. Strict inequality -- exactly at the
	 * threshold is NOT significant.
	 */
	int significant;
};

/*
 * One-pass population statistics over `n` baseline counts
 * (missing-function baselines contribute 0 -- the caller folds
 * that in before calling).
 *
 * Returns 0 and fills `out`; -1 on invalid input (n == 0 or
 * out == NULL).
 */
int diff_stats_compute(const uint64_t *baseline_counts, size_t n,
		       uint64_t test_count, double threshold,
		       struct diff_stats *out);

#endif /* RETRACE_DIFF_STATS_H_ */
