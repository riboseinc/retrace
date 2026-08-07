/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the filter action (TODO.complete/20).
 *
 * The action evaluates a single param comparison. If false, the
 * script aborts (action returns -1). If true, the script
 * continues (returns 0).
 *
 * Tests cover all six operators, param lookup, and edge cases.
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

static JSON_Object *build_filter_params(const char *param_name,
					const char *op, double value)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, "param_name", param_name);
	json_object_set_string(root, "op", op);
	json_object_set_number(root, "value", value);
	return root;
}

static struct ThreadContext *build_ctx_with_int_param(const char *name,
						      long val)
{
	static struct ThreadContext ctx;
	static struct FuncPrototype proto;
	struct FuncParam params[8];

	memset(&ctx, 0, sizeof(ctx));
	memset(params, 0, sizeof(params));
	memset(&proto, 0, sizeof(proto));

	strncpy(proto.name, "test_func", sizeof(proto.name) - 1);
	ctx.prototype = &proto;

	strncpy(params[0].param_meta.name, name,
		sizeof(params[0].param_meta.name) - 1);
	params[0].param_meta.direction = PDIR_IN;
	params[0].val = val;

	ctx.params_cnt = 1;
	memcpy(ctx.params, params, sizeof(params));
	return &ctx;
}

static struct ThreadContext *build_empty_ctx(void)
{
	static struct ThreadContext ctx;
	static struct FuncPrototype proto;

	memset(&ctx, 0, sizeof(ctx));
	memset(&proto, 0, sizeof(proto));
	strncpy(proto.name, "test_func", sizeof(proto.name) - 1);
	ctx.prototype = &proto;
	return &ctx;
}

static void test_action_lookup(void)
{
	retrace_actions_init();
	assert(retrace_actions_get("filter") != NULL);
}

static void test_eq_pass(void)
{
	action_fn_t action = retrace_actions_get("filter");
	struct ThreadContext *ctx = build_ctx_with_int_param("x", 42);
	JSON_Object *p = build_filter_params("x", "==", 42);

	assert(action(ctx, p) == 0);
	json_value_free(json_object_get_wrapping_value(p));
}

static void test_eq_fail(void)
{
	action_fn_t action = retrace_actions_get("filter");
	struct ThreadContext *ctx = build_ctx_with_int_param("x", 42);
	JSON_Object *p = build_filter_params("x", "==", 99);

	assert(action(ctx, p) == -1);
	json_value_free(json_object_get_wrapping_value(p));
}


#define DEFINE_OP_TEST(testname, op, actual, expected, should_pass) \
static void test_##testname(void) \
{ \
	action_fn_t action = retrace_actions_get("filter"); \
	struct ThreadContext *ctx = build_ctx_with_int_param("x", actual); \
	JSON_Object *p = build_filter_params("x", op, expected); \
	int rc = action(ctx, p); \
	assert(should_pass ? rc == 0 : rc == -1); \
	json_value_free(json_object_get_wrapping_value(p)); \
}

DEFINE_OP_TEST(ne_pass, "!=", 42, 99, 1)
DEFINE_OP_TEST(ne_fail, "!=", 42, 42, 0)
DEFINE_OP_TEST(gt_pass, ">", 42, 10, 1)
DEFINE_OP_TEST(gt_fail, ">", 42, 99, 0)
DEFINE_OP_TEST(lt_pass, "<", 42, 99, 1)
DEFINE_OP_TEST(lt_fail, "<", 42, 10, 0)
DEFINE_OP_TEST(ge_pass, ">=", 42, 42, 1)
DEFINE_OP_TEST(le_pass, "<=", 42, 42, 1)

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("filter");
	struct ThreadContext *ctx = build_empty_ctx();

	assert(action(ctx, NULL) == -1);
}

static void test_missing_param_name(void)
{
	action_fn_t action = retrace_actions_get("filter");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, "op", "==");
	json_object_set_number(root, "value", 0);
	assert(action(ctx, root) == -1);
	json_value_free(root_val);
}

static void test_missing_op(void)
{
	action_fn_t action = retrace_actions_get("filter");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, "param_name", "x");
	json_object_set_number(root, "value", 0);
	assert(action(ctx, root) == -1);
	json_value_free(root_val);
}

static void test_unknown_param(void)
{
	action_fn_t action = retrace_actions_get("filter");
	struct ThreadContext *ctx = build_ctx_with_int_param("real", 42);
	JSON_Object *p = build_filter_params("nonexistent", "==", 42);

	assert(action(ctx, p) == -1);
	json_value_free(json_object_get_wrapping_value(p));
}

static void test_unknown_op(void)
{
	action_fn_t action = retrace_actions_get("filter");
	struct ThreadContext *ctx = build_ctx_with_int_param("x", 42);
	JSON_Object *p = build_filter_params("x", "invalid_op", 42);

	assert(action(ctx, p) == -1);
	json_value_free(json_object_get_wrapping_value(p));
}

static void test_negative_value(void)
{
	action_fn_t action = retrace_actions_get("filter");
	struct ThreadContext *ctx = build_ctx_with_int_param("x", -5);
	JSON_Object *p = build_filter_params("x", "<", 0);

	assert(action(ctx, p) == 0);
	json_value_free(json_object_get_wrapping_value(p));
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

	printf("filter action tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_param_name);
	TEST(missing_op);
	TEST(unknown_param);
	TEST(unknown_op);

	printf("  -- operators --\n");
	TEST(eq_pass);
	TEST(eq_fail);
	TEST(ne_pass);
	TEST(ne_fail);
	TEST(gt_pass);
	TEST(gt_fail);
	TEST(lt_pass);
	TEST(lt_fail);
	TEST(ge_pass);
	TEST(le_pass);

	printf("  -- edge cases --\n");
	TEST(negative_value);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
