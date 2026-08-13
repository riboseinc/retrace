/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_DIFF_THRESHOLD_H_
#define RETRACE_DIFF_THRESHOLD_H_

#include <stdint.h>

/*
 * Threshold math for the differential trace analysis (TODO.complete/27).
 *
 * Owns the predicate "does this before/after change count as a diff
 * above the configured threshold?" -- isolated from diff.c so it
 * can be unit-tested without spinning up the full diff pipeline.
 *
 * Semantics:
 *   - Identical before/after is never a diff (returns 0).
 *   - threshold_pct <= 0 means "report any change at all" (default).
 *   - 0 -> N is unbounded growth; always report when before == 0
 *     and after != 0 (mirrors how 0/0 is undefined, so we err on
 *     the side of reporting).
 *   - Otherwise: |delta| / before * 100 > threshold_pct.
 *
 * Returns 1 if the change should be reported, 0 otherwise.
 */
int diff_exceeds_threshold(uint64_t before, uint64_t after,
			   double threshold_pct);

#endif /* RETRACE_DIFF_THRESHOLD_H_ */
