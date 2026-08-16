/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "stats.h"

#include <math.h>

int diff_stats_compute(const uint64_t *baseline_counts, size_t n,
		       uint64_t test_count, double threshold,
		       struct diff_stats *out)
{
	double sum = 0;
	double ssd = 0;  /* sum of squared deviations from the mean */
	size_t i;

	if (baseline_counts == NULL || n == 0 || out == NULL)
		return -1;

	for (i = 0; i < n; i++)
		sum += (double)baseline_counts[i];

	out->mean = sum / (double)n;

	/* Two-pass variance: numerically stable where the one-pass
	 * E[x^2] - mean^2 form suffers catastrophic cancellation for
	 * large counts (call counts can exceed 2^53 precision).
	 */
	for (i = 0; i < n; i++) {
		double d = (double)baseline_counts[i] - out->mean;

		ssd += d * d;
	}
	out->stddev = sqrt(ssd / (double)n);

	if (out->stddev == 0) {
		out->no_variance = 1;
		out->z = 0;
		out->significant = ((double)test_count != out->mean);
	} else {
		out->no_variance = 0;
		out->z = ((double)test_count - out->mean) / out->stddev;
		out->significant = fabs(out->z) > threshold;
	}

	return 0;
}
