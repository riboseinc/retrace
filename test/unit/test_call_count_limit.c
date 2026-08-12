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

#include "test_utils.h"

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

DECLARE_TEST_STATE();

static void test_action_lookup(void)
{
	retrace_actions_init();
	CHECK(retrace_actions_get("call_count_limit") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx = build_ctx_for_func("test_null_params");
	int rc;

	rc = action(ctx, NULL);
	CHECK(rc == -1);
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
	CHECK(rc == -1);

	json_value_free(root_val);
}

static void test_limit_three_first_three_pass(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx = build_ctx_for_func("test_limit_3_pass");
	JSON_Object *params = build_json_number("limit", 3.0);
	int rc;
	int i;

	for (i = 0; i < 3; i++) {
		rc = action(ctx, params);
		CHECK(rc == 0);
	}

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_limit_three_fourth_aborts(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx =
		build_ctx_for_func("test_limit_3_abort");
	JSON_Object *params = build_json_number("limit", 3.0);
	int rc;
	int i;

	for (i = 0; i < 3; i++) {
		rc = action(ctx, params);
		CHECK(rc == 0);
	}

	rc = action(ctx, params);
	CHECK(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_limit_one_first_passes(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx = build_ctx_for_func("test_limit_1_pass");
	JSON_Object *params = build_json_number("limit", 1.0);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_limit_one_second_aborts(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx = build_ctx_for_func("test_limit_1_abort");
	JSON_Object *params = build_json_number("limit", 1.0);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == 0);
	rc = action(ctx, params);
	CHECK(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_independent_functions(void)
{
	static struct ThreadContext ctx_a_storage;
	static struct ThreadContext ctx_b_storage;
	static struct FuncPrototype proto_a;
	static struct FuncPrototype proto_b;
	struct ThreadContext *ctx_a = &ctx_a_storage;
	struct ThreadContext *ctx_b = &ctx_b_storage;
	action_fn_t action = retrace_actions_get("call_count_limit");
	JSON_Object *params = build_json_number("limit", 2.0);
	int rc;

	memset(&ctx_a_storage, 0, sizeof(ctx_a_storage));
	memset(&ctx_b_storage, 0, sizeof(ctx_b_storage));
	memset(&proto_a, 0, sizeof(proto_a));
	memset(&proto_b, 0, sizeof(proto_b));

	strncpy(proto_a.name, "indep_func_a", sizeof(proto_a.name) - 1);
	strncpy(proto_b.name, "indep_func_b", sizeof(proto_b.name) - 1);
	ctx_a->prototype = &proto_a;
	ctx_b->prototype = &proto_b;

	rc = action(ctx_a, params);
	CHECK(rc == 0);
	rc = action(ctx_a, params);
	CHECK(rc == 0);

	rc = action(ctx_b, params);
	CHECK(rc == 0);
	rc = action(ctx_b, params);
	CHECK(rc == 0);

	rc = action(ctx_a, params);
	CHECK(rc == -1);

	rc = action(ctx_b, params);
	CHECK(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_reclaim_with_different_limit_keeps_original(void)
{
	action_fn_t action = retrace_actions_get("call_count_limit");
	struct ThreadContext *ctx =
		build_ctx_for_func("reclaim_func");
	JSON_Object *p1 = build_json_number("limit", 5.0);
	JSON_Object *p2 = build_json_number("limit", 100.0);
	int rc;
	int i;

	rc = action(ctx, p1);
	CHECK(rc == 0);
	rc = action(ctx, p1);
	CHECK(rc == 0);
	rc = action(ctx, p1);
	CHECK(rc == 0);

	for (i = 0; i < 2; i++) {
		rc = action(ctx, p2);
		CHECK(rc == 0);
	}

	rc = action(ctx, p2);
	CHECK(rc == -1);

	json_value_free(json_object_get_wrapping_value(p1));
	json_value_free(json_object_get_wrapping_value(p2));
}

int main(void)
{
	init_minimal_real_impls();
	INIT_TESTS();

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

	return finish_tests(tests_run, tests_pass, tests_fail);
}
