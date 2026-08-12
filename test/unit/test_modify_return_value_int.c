/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the modify_return_value_int action.
 *
 * The action is the simplest of the built-ins: it reads retval_int
 * from action_params and writes it to t_ctx->ret_val. Verifies:
 *   - Action lookup succeeds
 *   - NULL params -> -1
 *   - Missing retval_int -> -1
 *   - Numeric value (zero, positive, negative, large) is set verbatim
 *   - Repeated calls overwrite (no accumulate)
 *
 * Part of TODO.complete/14.
 */

#include "test_utils.h"

#include <limits.h>

static struct ThreadContext *build_empty_ctx(void)
{
	static struct ThreadContext ctx;

	memset(&ctx, 0, sizeof(ctx));
	ctx.ret_val = 0;
	return &ctx;
}

DECLARE_TEST_STATE();

static void test_action_lookup(void)
{
	retrace_actions_init();
	CHECK(retrace_actions_get("modify_return_value_int") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("modify_return_value_int");
	struct ThreadContext *ctx = build_empty_ctx();
	int rc;

	rc = action(ctx, NULL);
	CHECK(rc == -1);
}

static void test_missing_retval_int(void)
{
	action_fn_t action = retrace_actions_get("modify_return_value_int");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	int rc;

	json_object_set_string(root, "unrelated", "foo");

	rc = action(ctx, root);
	CHECK(rc == -1);

	json_value_free(root_val);
}

static void test_set_zero(void)
{
	action_fn_t action = retrace_actions_get("modify_return_value_int");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Object *params = build_json_number("retval_int", 0.0);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == 0);
	CHECK(ctx->ret_val == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_set_positive(void)
{
	action_fn_t action = retrace_actions_get("modify_return_value_int");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Object *params = build_json_number("retval_int", 42.0);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == 0);
	CHECK(ctx->ret_val == 42);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_set_negative(void)
{
	action_fn_t action = retrace_actions_get("modify_return_value_int");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Object *params = build_json_number("retval_int", -13.0);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == 0);
	CHECK(ctx->ret_val == -13);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_set_large(void)
{
	action_fn_t action = retrace_actions_get("modify_return_value_int");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Object *params;
	int rc;

	params = build_json_number("retval_int", 9007199254740992.0);

	rc = action(ctx, params);
	CHECK(rc == 0);
	CHECK(ctx->ret_val == 9007199254740992L);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_overwrites_each_call(void)
{
	action_fn_t action = retrace_actions_get("modify_return_value_int");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Object *p1 = build_json_number("retval_int", 100.0);
	JSON_Object *p2 = build_json_number("retval_int", 200.0);
	int rc;

	rc = action(ctx, p1);
	CHECK(rc == 0);
	CHECK(ctx->ret_val == 100);

	rc = action(ctx, p2);
	CHECK(rc == 0);
	CHECK(ctx->ret_val == 200);

	json_value_free(json_object_get_wrapping_value(p1));
	json_value_free(json_object_get_wrapping_value(p2));
}

int main(void)
{
	init_minimal_real_impls();
	INIT_TESTS();

	printf("modify_return_value_int tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_retval_int);

	printf("  -- value semantics --\n");
	TEST(set_zero);
	TEST(set_positive);
	TEST(set_negative);
	TEST(set_large);
	TEST(overwrites_each_call);

	return finish_tests(tests_run, tests_pass, tests_fail);
}
