/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * fail_first -- fail the first N invocations, then recover.
 *
 * Cases:
 *   - The action registers under its name
 *   - Missing params (null, no fails, no retval_int) -> -1
 *   - Window edges: fails=2 -> calls 1,2 abort with the fault
 *     ret_val set and real never called; call 3+ returns 0 and
 *     leaves ret_val alone (the script's call_real owns it)
 *   - fails=0 is inert (every call passes through)
 *   - Independent functions tracked separately
 *   - Re-claiming an existing name with different fails warns
 *     but keeps the original window
 */

#include "test_utils.h"
#include "parson.h"

static JSON_Object *build_ff_params(double fails, double retval)
{
	JSON_Value *v = json_value_init_object();
	JSON_Object *o = json_value_get_object(v);

	json_object_set_number(o, "fails", fails);
	json_object_set_number(o, "retval_int", retval);
	return o;
}

/* two slots: tests that compare functions side by side need
 * distinct contexts (the counter table keys on the prototype
 * name, so aliasing one static ctx would alias the counters)
 */
static struct ThreadContext *build_ctx_for_func(int slot,
	const char *func_name)
{
	static struct ThreadContext ctx[2];
	static struct FuncPrototype proto[2];

	memset(&ctx[slot], 0, sizeof(ctx[slot]));
	memset(&proto[slot], 0, sizeof(proto[slot]));

	strncpy(proto[slot].name, func_name,
		sizeof(proto[slot].name) - 1);
	proto[slot].name[sizeof(proto[slot].name) - 1] = '\0';
	ctx[slot].prototype = &proto[slot];

	return &ctx[slot];
}

DECLARE_TEST_STATE();

static void test_action_lookup(void)
{
	retrace_actions_init();
	CHECK(retrace_actions_get("fail_first") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("fail_first");
	struct ThreadContext *ctx = build_ctx_for_func(0, "ff_null_params");
	int rc;

	rc = action(ctx, NULL);
	CHECK(rc == -1);
}

static void test_missing_params(void)
{
	action_fn_t action = retrace_actions_get("fail_first");
	struct ThreadContext *ctx;
	JSON_Value *root_val;
	JSON_Object *root;
	int rc;

	ctx = build_ctx_for_func(0, "ff_missing_fails");
	root_val = json_value_init_object();
	root = json_value_get_object(root_val);
	json_object_set_number(root, "retval_int", -11.0);
	rc = action(ctx, root);
	CHECK(rc == -1);
	json_value_free(root_val);

	ctx = build_ctx_for_func(0, "ff_missing_retval");
	root_val = json_value_init_object();
	root = json_value_get_object(root_val);
	json_object_set_number(root, "fails", 2.0);
	rc = action(ctx, root);
	CHECK(rc == -1);
	json_value_free(root_val);
}

static void test_window_edges(void)
{
	action_fn_t action = retrace_actions_get("fail_first");
	struct ThreadContext *ctx = build_ctx_for_func(0, "ff_window");
	JSON_Object *params;

	params = build_ff_params(2.0, -11.0);

	/* call 1: inside the window -- the fault is set, script
	 * aborts (call_real never runs)
	 */
	ctx->ret_val = 0xdead;
	CHECK(action(ctx, params) == -1);
	CHECK(ctx->ret_val == -11);

	/* call 2: still inside */
	CHECK(action(ctx, params) == -1);
	CHECK(ctx->ret_val == -11);

	/* call 3: recovered -- the action passes (0) and does NOT
	 * touch ret_val (the script's call_real owns it now)
	 */
	ctx->ret_val = 0xdead;
	CHECK(action(ctx, params) == 0);
	CHECK(ctx->ret_val == 0xdead);

	/* call 4 and on: stays recovered */
	CHECK(action(ctx, params) == 0);
	CHECK(ctx->ret_val == 0xdead);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_zero_fails_inert(void)
{
	action_fn_t action = retrace_actions_get("fail_first");
	struct ThreadContext *ctx = build_ctx_for_func(0, "ff_zero");
	JSON_Object *params;

	params = build_ff_params(0.0, -1.0);
	ctx->ret_val = 0xdead;
	CHECK(action(ctx, params) == 0);
	CHECK(ctx->ret_val == 0xdead);
	CHECK(action(ctx, params) == 0);
	CHECK(ctx->ret_val == 0xdead);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_independent_functions(void)
{
	action_fn_t a = retrace_actions_get("fail_first");
	struct ThreadContext *alpha = build_ctx_for_func(0, "ff_alpha");
	struct ThreadContext *beta = build_ctx_for_func(1, "ff_beta");
	JSON_Object *params;

	params = build_ff_params(1.0, -22.0);

	CHECK(a(alpha, params) == -1);
	CHECK(alpha->ret_val == -22);
	/* beta has its own window */
	CHECK(a(beta, params) == -1);
	CHECK(beta->ret_val == -22);
	/* both recovered now */
	CHECK(a(alpha, params) == 0);
	CHECK(a(beta, params) == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_reclaim_keeps_original_window(void)
{
	action_fn_t a = retrace_actions_get("fail_first");
	struct ThreadContext *ctx = build_ctx_for_func(0, "ff_reclaim");
	JSON_Object *first;
	JSON_Object *second;

	first = build_ff_params(1.0, -1.0);
	second = build_ff_params(5.0, -2.0);

	CHECK(a(ctx, first) == -1);
	/* the re-claim with fails=5 is ignored: the window was 1 */
	CHECK(a(ctx, second) == 0);

	json_value_free(json_object_get_wrapping_value(first));
	json_value_free(json_object_get_wrapping_value(second));
}

DECLARE_TEST_STATE();

int main(void)
{
	printf("  -- registration --\n");
	TEST(action_lookup);
	printf("  -- params --\n");
	TEST(params_null);
	TEST(missing_params);
	printf("  -- the fault window --\n");
	TEST(window_edges);
	TEST(zero_fails_inert);
	printf("  -- ownership --\n");
	TEST(independent_functions);
	TEST(reclaim_keeps_original_window);

	return finish_tests(tests_run, tests_pass, tests_fail);
}
