/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the incomplete_io action.
 *
 * The action truncates t_ctx->ret_val to (real_ret * rate), where
 * rate is clamped to [0.0, 1.0]. Negative or zero ret_vals pass
 * through untouched.
 *
 * Tests:
 *   - Action lookup succeeds
 *   - NULL params -> -1
 *   - Missing rate -> -1
 *   - rate=1.0 -> ret_val unchanged
 *   - rate=0.0 -> ret_val = 0
 *   - rate=0.5 -> ret_val halved (rounded down)
 *   - rate<0 clamped to 0
 *   - rate>1 clamped to 1
 *   - Negative ret_val passes through
 *   - Zero ret_val passes through
 *
 * Part of TODO.complete/14.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

static JSON_Object *build_params_with_rate(double rate)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_number(root, "rate", rate);
	return root;
}

static JSON_Object *build_empty_params(void)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, "unrelated", "foo");
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
	assert(retrace_actions_get("incomplete_io") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("incomplete_io");
	struct ThreadContext *ctx = build_ctx_with_retval(100);
	int rc;

	rc = action(ctx, NULL);
	assert(rc == -1);
}

static void test_missing_rate(void)
{
	action_fn_t action = retrace_actions_get("incomplete_io");
	struct ThreadContext *ctx = build_ctx_with_retval(100);
	JSON_Object *params = build_empty_params();
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_rate_one_full(void)
{
	action_fn_t action = retrace_actions_get("incomplete_io");
	struct ThreadContext *ctx = build_ctx_with_retval(100);
	JSON_Object *params = build_params_with_rate(1.0);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(ctx->ret_val == 100);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_rate_zero_eof(void)
{
	action_fn_t action = retrace_actions_get("incomplete_io");
	struct ThreadContext *ctx = build_ctx_with_retval(100);
	JSON_Object *params = build_params_with_rate(0.0);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(ctx->ret_val == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_rate_half_truncates(void)
{
	action_fn_t action = retrace_actions_get("incomplete_io");
	struct ThreadContext *ctx = build_ctx_with_retval(100);
	JSON_Object *params = build_params_with_rate(0.5);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(ctx->ret_val == 50);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_rate_negative_clamped(void)
{
	action_fn_t action = retrace_actions_get("incomplete_io");
	struct ThreadContext *ctx = build_ctx_with_retval(100);
	JSON_Object *params = build_params_with_rate(-0.5);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(ctx->ret_val == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_rate_above_one_clamped(void)
{
	action_fn_t action = retrace_actions_get("incomplete_io");
	struct ThreadContext *ctx = build_ctx_with_retval(100);
	JSON_Object *params = build_params_with_rate(2.5);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(ctx->ret_val == 100);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_negative_retval_passes_through(void)
{
	action_fn_t action = retrace_actions_get("incomplete_io");
	struct ThreadContext *ctx = build_ctx_with_retval(-1);
	JSON_Object *params = build_params_with_rate(0.5);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(ctx->ret_val == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_zero_retval_passes_through(void)
{
	action_fn_t action = retrace_actions_get("incomplete_io");
	struct ThreadContext *ctx = build_ctx_with_retval(0);
	JSON_Object *params = build_params_with_rate(0.5);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(ctx->ret_val == 0);

	json_value_free(json_object_get_wrapping_value(params));
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

	printf("incomplete_io tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_rate);

	printf("  -- rate semantics --\n");
	TEST(rate_one_full);
	TEST(rate_zero_eof);
	TEST(rate_half_truncates);
	TEST(rate_negative_clamped);
	TEST(rate_above_one_clamped);

	printf("  -- ret_val edge cases --\n");
	TEST(negative_retval_passes_through);
	TEST(zero_retval_passes_through);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
