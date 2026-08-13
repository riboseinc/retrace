/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "threshold.h"

int diff_exceeds_threshold(uint64_t before, uint64_t after,
			   double threshold_pct)
{
	double delta;
	double pct;

	if (before == after)
		return 0;
	if (threshold_pct <= 0.0)
		return 1;
	if (before == 0)
		return 1;  /* 0 -> N is unbounded; always report */

	delta = (double)((long long)after - (long long)before);
	pct = 100.0 * (delta < 0 ? -delta : delta) / (double)before;
	return pct > threshold_pct;
}
