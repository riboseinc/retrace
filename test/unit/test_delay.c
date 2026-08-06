/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the delay action.
 *
 * The action sleeps for the configured number of milliseconds.
 * Verification focuses on params validation and the return code,
 * not on actual sleep duration (a timing-based assertion would
 * be flaky on loaded CI runners).
 *
 * Tests:
 *   - Action lookup succeeds
 *   - NULL params -> 0 (no-op, no delay)
 *   - Missing "ms" -> 0 (no-op)
 *   - Zero ms -> 0 (no-op)
 *   - Negative ms -> 0 (no-op)
 *   - Small positive ms -> 0 (sleeps briefly)
 *
 * Part of TODO.complete/14.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

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

static JSON_Object *build_params_with_ms(double ms)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_number(root, "ms", ms);
	return root;
}

static JSON_Object *build_empty_params(void)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, "unrelated", "foo");
	return root;
}

static void test_action_lookup(void)
{
	retrace_actions_init();
	assert(retrace_actions_get("delay") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("delay");
	struct ThreadContext ctx;
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	rc = action(&ctx, NULL);
	assert(rc == 0);
}

static void test_missing_ms(void)
{
	action_fn_t action = retrace_actions_get("delay");
	struct ThreadContext ctx;
	JSON_Object *params = build_empty_params();
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	rc = action(&ctx, params);
	assert(rc == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_zero_ms(void)
{
	action_fn_t action = retrace_actions_get("delay");
	struct ThreadContext ctx;
	JSON_Object *params = build_params_with_ms(0.0);
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	rc = action(&ctx, params);
	assert(rc == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_negative_ms(void)
{
	action_fn_t action = retrace_actions_get("delay");
	struct ThreadContext ctx;
	JSON_Object *params = build_params_with_ms(-100.0);
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	rc = action(&ctx, params);
	assert(rc == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_small_positive_ms(void)
{
	action_fn_t action = retrace_actions_get("delay");
	struct ThreadContext ctx;
	JSON_Object *params = build_params_with_ms(1.0);
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	rc = action(&ctx, params);
	assert(rc == 0);

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

	printf("delay tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_ms);
	TEST(zero_ms);
	TEST(negative_ms);

	printf("  -- valid delays --\n");
	TEST(small_positive_ms);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
