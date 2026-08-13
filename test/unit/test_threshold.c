/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for diff_exceeds_threshold (TODO.complete/27).
 *
 * The predicate gates CI regression reports from retrace-diff
 * (recipe 25). Wrong answers here mean either false alarms in
 * clean PRs or missed regressions in dirty PRs.
 *
 * Covers:
 *   - identical values never report (return 0)
 *   - threshold_pct <= 0 reports any change (the default mode)
 *   - 0 -> N is unbounded; always reports
 *   - N -> 0 reports for any reasonable threshold
 *   - small change vs threshold on either side of the cutoff
 *   - negative-direction changes (after < before) are absolute
 *   - large threshold suppresses everything except unbounded cases
 *   - 0 -> 0 is a no-op (identical, returns 0)
 */

#include "threshold.h"

#include <stdio.h>

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

/* ----- identical values ----- */

static void test_identical_values_never_report(void)
{
	CHECK(diff_exceeds_threshold(100, 100, 0.0) == 0);
	CHECK(diff_exceeds_threshold(100, 100, 1.0) == 0);
	CHECK(diff_exceeds_threshold(100, 100, 99.0) == 0);
}

static void test_zero_to_zero_is_not_a_diff(void)
{
	CHECK(diff_exceeds_threshold(0, 0, 0.0) == 0);
	CHECK(diff_exceeds_threshold(0, 0, 50.0) == 0);
}

/* ----- threshold_pct <= 0: report any change ----- */

static void test_zero_threshold_reports_any_change(void)
{
	CHECK(diff_exceeds_threshold(100, 101, 0.0) == 1);
	CHECK(diff_exceeds_threshold(100, 99, 0.0) == 1);
	CHECK(diff_exceeds_threshold(1, 2, 0.0) == 1);
}

static void test_negative_threshold_treated_as_report_any(void)
{
	CHECK(diff_exceeds_threshold(100, 101, -1.0) == 1);
}

/* ----- 0 -> N: unbounded ----- */

static void test_zero_to_n_always_reports(void)
{
	CHECK(diff_exceeds_threshold(0, 1, 99.0) == 1);
	CHECK(diff_exceeds_threshold(0, 1, 1000.0) == 1);
	CHECK(diff_exceeds_threshold(0, 1000, 99.0) == 1);
}

/* ----- positive direction ----- */

static void test_positive_change_above_threshold(void)
{
	CHECK(diff_exceeds_threshold(100, 200, 50.0) == 1);   /* +100% > 50% */
	CHECK(diff_exceeds_threshold(100, 150, 25.0) == 1);   /* +50% > 25% */
	CHECK(diff_exceeds_threshold(100, 101, 0.5) == 1);    /* +1% > 0.5% */
}

static void test_positive_change_at_threshold_does_not_report(void)
{
	/* Strict >: exactly-at-threshold does not report. */
	CHECK(diff_exceeds_threshold(100, 150, 50.0) == 0);   /* +50% == 50% */
	CHECK(diff_exceeds_threshold(100, 200, 100.0) == 0);  /* +100% == 100% */
}

static void test_positive_change_below_threshold(void)
{
	CHECK(diff_exceeds_threshold(100, 101, 5.0) == 0);    /* +1% < 5% */
	CHECK(diff_exceeds_threshold(100, 110, 50.0) == 0);   /* +10% < 50% */
}

/* ----- negative direction (after < before) ----- */

static void test_negative_change_at_threshold_does_not_report(void)
{
	/* |delta|/before = 50/100 = 50%. Strict >: 50 > 50 false. */
	CHECK(diff_exceeds_threshold(100, 50, 50.0) == 0);
	CHECK(diff_exceeds_threshold(100, 75, 25.0) == 0);
}

static void test_negative_change_above_threshold(void)
{
	/* |delta|/before = 50/100 = 50%. > 25% reports. */
	CHECK(diff_exceeds_threshold(100, 50, 25.0) == 1);
	/* |delta|/before = 1/100 = 1%. > 0.5% reports. */
	CHECK(diff_exceeds_threshold(100, 99, 0.5) == 1);
}

static void test_negative_change_below_threshold(void)
{
	CHECK(diff_exceeds_threshold(100, 99, 5.0) == 0);     /* 1% < 5% */
	CHECK(diff_exceeds_threshold(100, 90, 50.0) == 0);    /* 10% < 50% */
}

/* ----- N -> 0 ----- */

static void test_n_to_zero_at_100pct_does_not_report(void)
{
	/* before=100, after=0: |delta|=100, pct=100%. 100 > 100 false. */
	CHECK(diff_exceeds_threshold(100, 0, 100.0) == 0);
}

static void test_n_to_zero_below_100pct_threshold_reports(void)
{
	CHECK(diff_exceeds_threshold(100, 0, 99.0) == 1);
	CHECK(diff_exceeds_threshold(100, 0, 50.0) == 1);
	CHECK(diff_exceeds_threshold(100, 0, 1.0) == 1);
}

/* ----- large-threshold suppression ----- */

static void test_large_threshold_suppresses_typical_changes(void)
{
	CHECK(diff_exceeds_threshold(100, 200, 200.0) == 0);  /* +100% < 200% */
	CHECK(diff_exceeds_threshold(100, 50, 200.0) == 0);   /* -50% < 200% */
	CHECK(diff_exceeds_threshold(1000, 1500, 200.0) == 0); /* +50% < 200% */
}

/* ----- boundary: before == 0 with after == 0 is handled by equal check ----- */

static void test_before_zero_after_zero_short_circuits(void)
{
	/* Hits the `before == after` branch first, returns 0. */
	CHECK(diff_exceeds_threshold(0, 0, 0.0) == 0);
	CHECK(diff_exceeds_threshold(0, 0, 100.0) == 0);
}

int main(void)
{
	printf("-- identical values --\n");
	TEST(identical_values_never_report);
	TEST(zero_to_zero_is_not_a_diff);

	printf("-- threshold_pct <= 0 --\n");
	TEST(zero_threshold_reports_any_change);
	TEST(negative_threshold_treated_as_report_any);

	printf("-- 0 -> N: unbounded --\n");
	TEST(zero_to_n_always_reports);

	printf("-- positive direction --\n");
	TEST(positive_change_above_threshold);
	TEST(positive_change_at_threshold_does_not_report);
	TEST(positive_change_below_threshold);

	printf("-- negative direction --\n");
	TEST(negative_change_at_threshold_does_not_report);
	TEST(negative_change_above_threshold);
	TEST(negative_change_below_threshold);

	printf("-- N -> 0 --\n");
	TEST(n_to_zero_at_100pct_does_not_report);
	TEST(n_to_zero_below_100pct_threshold_reports);

	printf("-- large-threshold suppression --\n");
	TEST(large_threshold_suppresses_typical_changes);

	printf("-- boundaries --\n");
	TEST(before_zero_after_zero_short_circuits);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
