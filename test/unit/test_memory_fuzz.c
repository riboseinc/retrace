/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the memory_fuzz action.
 *
 * The action fuzzes memory-allocation return values: with
 * probability fail_rate, sets ret_val=NULL + errno=ENOMEM
 * (simulating malloc failure).
 *
 * Tests:
 *   - Action lookup succeeds
 *   - NULL params -> -1
 *   - Missing fail_rate -> -1
 *   - fail_rate=0.0 -> never fuzzes (ret_val unchanged across many
 *     invocations)
 *   - fail_rate=1.0 -> always fuzzes (ret_val=NULL, errno=ENOMEM)
 *   - Deterministic with fuzz_seed (same seed -> same decision
 *     sequence)
 *
 * Note: memory_fuzz's `initialized` static flag persists across
 * tests in this binary. Tests order matters: seed-bearing tests
 * must run before no-seed tests, or the seed must be set fresh
 * each time. We use the action_params' fuzz_seed field which the
 * action honors on first call only.
 *
 * Part of TODO.complete/14.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "engine.h"
#include "actions.h"
#include "funcs.h"
#include "data_types.h"
#include "real_impls.h"
#include "parson.h"

typedef int (*action_fn_t)(struct ThreadContext *t_ctx,
			    const JSON_Object *action_params);

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

static JSON_Object *build_params(double fail_rate, int with_seed,
				  double fuzz_seed)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_number(root, "fail_rate", fail_rate);
	if (with_seed)
		json_object_set_number(root, "fuzz_seed", fuzz_seed);
	return root;
}

static struct ThreadContext *build_ctx_with_retval(long ret_val)
{
	static struct ThreadContext ctx;

	memset(&ctx, 0, sizeof(ctx));
	ctx.ret_val = ret_val;
	return &ctx;
}

static void test_action_lookup(void)
{
	retrace_actions_init();
	assert(retrace_actions_get("memory_fuzz") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("memory_fuzz");
	struct ThreadContext *ctx = build_ctx_with_retval(0x1000);
	int rc;

	rc = action(ctx, NULL);
	assert(rc == -1);
}

static void test_missing_fail_rate(void)
{
	action_fn_t action = retrace_actions_get("memory_fuzz");
	struct ThreadContext *ctx = build_ctx_with_retval(0x1000);
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	int rc;

	json_object_set_string(root, "unrelated", "foo");

	/* First call seeds srand but then errors on missing fail_rate.
	 * The init flag persists for the rest of the binary; that's
	 * fine, subsequent tests provide explicit fail_rate.
	 */
	rc = action(ctx, root);
	assert(rc == -1);

	json_value_free(root_val);
}

static void test_fail_rate_zero_never_fuzzes(void)
{
	action_fn_t action = retrace_actions_get("memory_fuzz");
	struct ThreadContext *ctx;
	JSON_Object *params = build_params(0.0, 1, 42.0);
	int rc;
	int i;

	/* fail_rate=0.0 means random_value <= 0 never holds. Verify
	 * across 50 invocations that ret_val is never touched.
	 */
	for (i = 0; i < 50; i++) {
		ctx = build_ctx_with_retval(0xDEAD);
		rc = action(ctx, params);
		assert(rc == 0);
		assert(ctx->ret_val == 0xDEAD);
	}

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_fail_rate_one_always_fuzzes(void)
{
	action_fn_t action = retrace_actions_get("memory_fuzz");
	struct ThreadContext *ctx;
	JSON_Object *params = build_params(1.0, 1, 99.0);
	int rc;
	int i;

	/* fail_rate=1.0 means random_value <= RAND_MAX always holds. */
	for (i = 0; i < 50; i++) {
		ctx = build_ctx_with_retval(0xDEAD);
		errno = 0;
		rc = action(ctx, params);
		assert(rc == 0);
		assert(ctx->ret_val == (long)NULL);
		assert(errno == ENOMEM);
	}

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_deterministic_with_seed(void)
{
	action_fn_t action = retrace_actions_get("memory_fuzz");

	/* Same seed + same fail_rate should produce the same decision
	 * sequence. The action's `initialized` flag persists, so we
	 * can't re-seed via fuzz_seed in this test. Instead, we use
	 * the fuzzing_seed action (which sets a global) and verify
	 * determinism that way.
	 *
	 * Setup: invoke fuzzing_seed with seed=42, then sample 20
	 * decisions at fail_rate=0.5. Reset and repeat. The two
	 * sequences should be identical.
	 */
	action_fn_t seed_action = retrace_actions_get("fuzzing_seed");
	JSON_Object *seed_params;
	JSON_Object *fuzz_params;
	int seq_a[20];
	int seq_b[20];
	int i;

	seed_params = json_value_get_object(json_value_init_object());
	json_object_set_number(seed_params, "seed", 42.0);
	(void)seed_action(build_ctx_with_retval(0), seed_params);

	fuzz_params = build_params(0.5, 0, 0.0);
	for (i = 0; i < 20; i++) {
		struct ThreadContext *ctx = build_ctx_with_retval(0xCAFE);

		action(ctx, fuzz_params);
		seq_a[i] = (ctx->ret_val == 0) ? 1 : 0;
	}

	/* Re-seed and re-sample. */
	(void)seed_action(build_ctx_with_retval(0), seed_params);
	for (i = 0; i < 20; i++) {
		struct ThreadContext *ctx = build_ctx_with_retval(0xCAFE);

		action(ctx, fuzz_params);
		seq_b[i] = (ctx->ret_val == 0) ? 1 : 0;
	}

	for (i = 0; i < 20; i++)
		assert(seq_a[i] == seq_b[i]);

	json_value_free(json_object_get_wrapping_value(seed_params));
	json_value_free(json_object_get_wrapping_value(fuzz_params));
}

int main(void)
{
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;
	retrace_real_impls.time = time;

	printf("memory_fuzz tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_fail_rate);

	printf("  -- boundary fail_rates --\n");
	TEST(fail_rate_zero_never_fuzzes);
	TEST(fail_rate_one_always_fuzzes);

	printf("  -- determinism --\n");
	TEST(deterministic_with_seed);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
