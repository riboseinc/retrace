/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the z-score stats (TODO.complete/27 P2).
 *
 * diff_stats_compute drives the --stats CI gate (cookbook 25):
 * a wrong mean/stddev/z means missed regressions or false
 * alarms on every PR.
 *
 * Covers:
 *   - mean and population stddev for known distributions
 *     ([10,12,14] -> 12 +- sqrt(8/3))
 *   - zero-variance detection + the any-deviation rule
 *   - all-zero baselines are a zero-variance case with mean 0
 *   - z threshold is STRICT (exactly-at-threshold not significant)
 *   - negative-direction z is absolute-valued for the decision
 *   - single baseline (n=1) is zero-variance
 *   - invalid input (n=0, NULL) returns -1
 *   - large counts do not lose the deviation
 *   - typical significant / not-significant classifications
 */

#include "stats.h"

#include <math.h>
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

static int approx(double a, double b)
{
	return fabs(a - b) < 1e-9;
}

/* ----- basic distribution stats ----- */

static void test_mean_and_stddev_known_distribution(void)
{
	const uint64_t base[] = {10, 12, 14};
	struct diff_stats st;

	CHECK(diff_stats_compute(base, 3, 13, 2.0, &st) == 0);
	CHECK(approx(st.mean, 12.0));
	/* variance = (100+144+196)/3 - 144 = 440/3 - 144 = 8/3 */
	CHECK(approx(st.stddev, sqrt(8.0 / 3.0)));
	CHECK(approx(st.z, (13.0 - 12.0) / sqrt(8.0 / 3.0)));
	CHECK(st.no_variance == 0);
	/* z ~ 0.61 -> not significant at 2.0. */
	CHECK(st.significant == 0);
}

static void test_significant_above_threshold(void)
{
	const uint64_t base[] = {10, 12, 14};
	struct diff_stats st;

	CHECK(diff_stats_compute(base, 3, 16, 2.0, &st) == 0);
	/* z = 4 / 1.633 ~ 2.449 > 2.0 */
	CHECK(st.significant == 1);
	CHECK(st.z > 2.0);
}

static void test_negative_direction_absolute(void)
{
	const uint64_t base[] = {10, 12, 14};
	struct diff_stats st;

	CHECK(diff_stats_compute(base, 3, 8, 2.0, &st) == 0);
	/* z = -4 / 1.633 ~ -2.449; |z| > 2.0 -> significant. */
	CHECK(approx(st.z, -4.0 / sqrt(8.0 / 3.0)));
	CHECK(st.significant == 1);
}

/* ----- zero variance ----- */

static void test_zero_variance_any_deviation_flags(void)
{
	const uint64_t base[] = {10, 10, 10};
	struct diff_stats st;

	CHECK(diff_stats_compute(base, 3, 10, 2.0, &st) == 0);
	CHECK(st.no_variance == 1);
	CHECK(approx(st.mean, 10.0));
	CHECK(approx(st.stddev, 0.0));
	/* Equal to the constant -> not flagged. */
	CHECK(st.significant == 0);

	CHECK(diff_stats_compute(base, 3, 11, 2.0, &st) == 0);
	CHECK(st.no_variance == 1);
	/* Any deviation (even 1) flags with no variance. */
	CHECK(st.significant == 1);
}

static void test_all_zero_baselines(void)
{
	const uint64_t base[] = {0, 0, 0, 0};
	struct diff_stats st;

	CHECK(diff_stats_compute(base, 4, 0, 2.0, &st) == 0);
	CHECK(st.no_variance == 1);
	CHECK(approx(st.mean, 0.0));
	CHECK(st.significant == 0);

	CHECK(diff_stats_compute(base, 4, 1, 2.0, &st) == 0);
	CHECK(st.significant == 1);
}

static void test_single_baseline_is_zero_variance(void)
{
	const uint64_t base[] = {5};
	struct diff_stats st;

	CHECK(diff_stats_compute(base, 1, 5, 2.0, &st) == 0);
	CHECK(st.no_variance == 1);
	CHECK(st.significant == 0);

	CHECK(diff_stats_compute(base, 1, 100, 2.0, &st) == 0);
	CHECK(st.significant == 1);
}

/* ----- threshold semantics ----- */

static void test_threshold_is_strict(void)
{
	/* Craft a distribution where |z| lands exactly on the
	 * threshold: baselines [0, 2] -> mean 1, stddev 1; test 3
	 * -> z = 2 exactly. Strict > means NOT significant at 2.0.
	 */
	const uint64_t base[] = {0, 2};
	struct diff_stats st;

	CHECK(diff_stats_compute(base, 2, 3, 2.0, &st) == 0);
	CHECK(approx(st.mean, 1.0));
	CHECK(approx(st.stddev, 1.0));
	CHECK(approx(st.z, 2.0));
	CHECK(st.significant == 0);

	/* Just above the threshold flags. */
	CHECK(diff_stats_compute(base, 2, 4, 2.0, &st) == 0);
	CHECK(st.z > 2.0);
	CHECK(st.significant == 1);
}

static void test_custom_threshold_widens_gate(void)
{
	const uint64_t base[] = {10, 12, 14};
	struct diff_stats st;

	/* z ~ 2.449: significant at 2.0, not at 3.0. */
	CHECK(diff_stats_compute(base, 3, 16, 2.0, &st) == 0);
	CHECK(st.significant == 1);
	CHECK(diff_stats_compute(base, 3, 16, 3.0, &st) == 0);
	CHECK(st.significant == 0);
}

/* ----- input validation + extremes ----- */

static void test_invalid_inputs_rejected(void)
{
	const uint64_t base[] = {1, 2};
	struct diff_stats st;

	CHECK(diff_stats_compute(NULL, 2, 1, 2.0, &st) == -1);
	CHECK(diff_stats_compute(base, 0, 1, 2.0, &st) == -1);
	CHECK(diff_stats_compute(base, 2, 1, 2.0, NULL) == -1);
}

static void test_large_counts(void)
{
	const uint64_t base[] = {1000000000ULL, 1000000002ULL};
	struct diff_stats st;

	CHECK(diff_stats_compute(base, 2, 1000000000ULL, 2.0, &st) == 0);
	/* mean 1e9+1, stddev 1; test at the mean -> z -1, not sig. */
	CHECK(approx(st.mean, 1000000001.0));
	CHECK(approx(st.stddev, 1.0));
	CHECK(st.significant == 0);

	/* +4 -> z ~ 3 -> significant. */
	CHECK(diff_stats_compute(base, 2, 1000000005ULL, 2.0, &st) == 0);
	CHECK(st.z > 3.0);
	CHECK(st.significant == 1);
}

int main(void)
{
	printf("-- distribution stats --\n");
	TEST(mean_and_stddev_known_distribution);
	TEST(significant_above_threshold);
	TEST(negative_direction_absolute);

	printf("-- zero variance --\n");
	TEST(zero_variance_any_deviation_flags);
	TEST(all_zero_baselines);
	TEST(single_baseline_is_zero_variance);

	printf("-- threshold semantics --\n");
	TEST(threshold_is_strict);
	TEST(custom_threshold_widens_gate);

	printf("-- validation + extremes --\n");
	TEST(invalid_inputs_rejected);
	TEST(large_counts);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
