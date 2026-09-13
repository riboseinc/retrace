/*
 * Unit: the drift-hit matcher (TODO.impl/18). Standalone by
 * the card-06 law: only drift_hits.c, no daemon/parson
 * linkage.
 *
 *   ctest -R unit-test-drift-hits
 */
#include <stdio.h>
#include <string.h>

#include "drift_hits.h"

static int tests_fail;

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

static void test_covered_is_not_a_hit(void)
{
	retrace_drift_hits_reset();
	retrace_drift_claim(100, "openat", "/cfg/ok.bin");
	CHECK(retrace_drift_observe(100, "openat", "/cfg/ok.bin")
		== 0);
}

static void test_uncovered_path_is_a_hit(void)
{
	retrace_drift_hits_reset();
	retrace_drift_claim(100, "openat", "/cfg/ok.bin");
	CHECK(retrace_drift_observe(100, "openat",
		"/etc/shadow") == 1);
}

static void test_hit_dedups_within_window(void)
{
	retrace_drift_hits_reset();
	CHECK(retrace_drift_observe(100, "openat",
		"/etc/shadow") == 1);
	CHECK(retrace_drift_observe(100, "openat",
		"/etc/shadow") == 0);	/* named already */
	CHECK(retrace_drift_observe(100, "openat",
		"/etc/passwd") == 1);	/* a different escape */
}

static void test_pidless_lane_matches(void)
{
	retrace_drift_hits_reset();
	/* the kernel lane often carries no workload pid */
	retrace_drift_claim(100, "openat", "/cfg/ok.bin");
	CHECK(retrace_drift_observe(0, "openat", "/cfg/ok.bin")
		== 0);
	/* but a CONFLICTING pid does not cover */
	retrace_drift_hits_reset();
	retrace_drift_claim(100, "openat", "/cfg/ok.bin");
	CHECK(retrace_drift_observe(200, "openat", "/cfg/ok.bin")
		== 1);
}

static void test_pathless_observation_never_hits(void)
{
	retrace_drift_hits_reset();
	CHECK(retrace_drift_observe(100, "openat", "") == 0);
	CHECK(retrace_drift_observe(100, "openat", NULL) == 0);
}

static void test_ring_overload_olds_out(void)
{
	int i;

	retrace_drift_hits_reset();
	/* bury the claim under 2x the ring: it must be gone */
	retrace_drift_claim(100, "openat", "/cfg/buried.bin");
	for (i = 0; i < 300; i++)
		retrace_drift_claim(100, "openat", "/cfg/x.bin");
	CHECK(retrace_drift_observe(100, "openat",
		"/cfg/buried.bin") == 1);
	/* the recent claims still cover */
	CHECK(retrace_drift_observe(100, "openat", "/cfg/x.bin")
		== 0);
}

static void test_claim_after_hit_recovers(void)
{
	retrace_drift_hits_reset();
	CHECK(retrace_drift_observe(100, "openat",
		"/cfg/late.bin") == 1);
	/* the claim arrives a moment later (window skew) */
	retrace_drift_claim(100, "openat", "/cfg/late.bin");
	CHECK(retrace_drift_observe(100, "openat",
		"/cfg/late.bin") == 0);
}

int main(void)
{
	test_covered_is_not_a_hit();
	test_uncovered_path_is_a_hit();
	test_hit_dedups_within_window();
	test_pidless_lane_matches();
	test_pathless_observation_never_hits();
	test_ring_overload_olds_out();
	test_claim_after_hit_recovers();
	printf("drift_hits: all checks passed\n");
	return tests_fail != 0;
}
