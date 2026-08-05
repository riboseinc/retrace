/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the call_count_limit action.
 *
 * The action maintains a per-function-name counter. The first N
 * invocations return 0; the (N+1)th returns -1, aborting the rest
 * of the intercept script.
 *
 * Verifies:
 *   - Action lookup succeeds
 *   - NULL params -> -1
 *   - Missing limit -> -1
 *   - First N calls return 0; (N+1)th returns -1
 *   - Independent functions tracked separately
 *   - Re-claiming an existing name with a different limit warns but
 *     keeps the original limit
 *
 * Note: the global counter table (g_entries in call_count_limit.c)
 * persists across tests in this binary. Tests use unique function
 * names to avoid collisions.
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

static JSON_Object *build_params_with_number(const char *key, double val)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_number(root, key, val);
	return root;
}

/* Build a ThreadContext with a prototype name. call_count_limit
 * keys its counter table on prototype->name.
 */
static struct ThreadContext *build_ctx_for_func(const char *func_name)
{
	static struct ThreadContext ctx;
	static struct FuncPrototype proto;

	memset(&ctx, 0, sizeof(ctx));
	memset(&proto, 0, sizeof(proto));

	strncpy(proto.name, func_name, sizeof(proto.name) - 1);
	proto.name[sizeof(proto.name) - 1] = '\0';
	ctx.prototype = &proto;

	return &ctx;
}

static void test_action_lookup(void)
{
	retrace_actions_init();
	assert(retrace_actions_get("call_count_limit") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx = build_ctx_for_func("test_null_params");
	int rc;

	rc = action(ctx, NULL);
	assert(rc == -1);
}

static void test_missing_limit(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx = build_ctx_for_func("test_missing_limit");
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	int rc;

	json_object_set_string(root, "unrelated", "foo");

	rc = action(ctx, root);
	assert(rc == -1);

	json_value_free(root_val);
}

static void test_limit_three_first_three_pass(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx = build_ctx_for_func("test_limit_3_pass");
	JSON_Object *params = build_params_with_number("limit", 3.0);
	int rc;
	int i;

	/* First 3 calls (count goes 1, 2, 3) should pass. */
	for (i = 0; i < 3; i++) {
		rc = action(ctx, params);
		assert(rc == 0);
	}

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_limit_three_fourth_aborts(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx =
		build_ctx_for_func("test_limit_3_abort");
	JSON_Object *params = build_params_with_number("limit", 3.0);
	int rc;
	int i;

	for (i = 0; i < 3; i++)
		action(ctx, params);

	/* 4th call (count=4) should abort. */
	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_limit_one_first_passes(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx = build_ctx_for_func("test_limit_1_pass");
	JSON_Object *params = build_params_with_number("limit", 1.0);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_limit_one_second_aborts(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx = build_ctx_for_func("test_limit_1_abort");
	JSON_Object *params = build_params_with_number("limit", 1.0);
	int rc;

	action(ctx, params);
	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_independent_functions(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx_a = build_ctx_for_func("indep_func_a");
	struct ThreadContext *ctx_b = build_ctx_for_func("indep_func_b");
	JSON_Object *params = build_params_with_number("limit", 2.0);
	int rc;

	/* A: 2 calls pass */
	rc = action(ctx_a, params);
	assert(rc == 0);
	rc = action(ctx_a, params);
	assert(rc == 0);

	/* B: independent counter, 2 calls pass */
	rc = action(ctx_b, params);
	assert(rc == 0);
	rc = action(ctx_b, params);
	assert(rc == 0);

	/* A: 3rd call now aborts */
	rc = action(ctx_a, params);
	assert(rc == -1);

	/* B: 3rd call also aborts (independent count, same limit) */
	rc = action(ctx_b, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_reclaim_with_different_limit_keeps_original(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx =
		build_ctx_for_func("reclaim_func");
	JSON_Object *p1 = build_params_with_number("limit", 5.0);
	JSON_Object *p2 = build_params_with_number("limit", 100.0);
	int rc;
	int i;

	/* Claim with limit=5, run 3 calls. */
	assert(action(ctx, p1) == 0);
	assert(action(ctx, p1) == 0);
	assert(action(ctx, p1) == 0);

	/* Re-claim same name with limit=100. Original limit (5) is kept. */
	for (i = 0; i < 2; i++)
		assert(action(ctx, p2) == 0);  /* count=4, 5; still under 5 */

	/* count=6: now aborts because original limit was 5. */
	rc = action(ctx, p2);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(p1));
	json_value_free(json_object_get_wrapping_value(p2));
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

	printf("call_count_limit tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_limit);

	printf("  -- count semantics --\n");
	TEST(limit_three_first_three_pass);
	TEST(limit_three_fourth_aborts);
	TEST(limit_one_first_passes);
	TEST(limit_one_second_aborts);

	printf("  -- per-function isolation --\n");
	TEST(independent_functions);

	printf("  -- reclaim semantics --\n");
	TEST(reclaim_with_different_limit_keeps_original);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
