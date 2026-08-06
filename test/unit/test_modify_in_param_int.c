/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the modify_in_param_int action.
 *
 * The action looks up a param by name in t_ctx->params, verifies
 * it's an IN param, optionally matches its current value against
 * match_int, and writes new_int into param->val.
 *
 * Tests:
 *   - Action lookup succeeds
 *   - NULL params -> -1
 *   - Missing param_name -> -1
 *   - Unknown param name -> -1
 *   - Param with PDIR_OUT direction -> -1
 *   - Missing new_int -> -1
 *   - Direct modification (no match_int)
 *   - match_int present and matches -> writes new value
 *   - match_int present, no match -> no-op
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

/* Build a JSON object with one required string + one required number
 * and one optional number.
 */
static JSON_Object *build_params_full(const char *param_name,
				       double new_int,
				       int with_match,
				       double match_int)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, "param_name", param_name);
	json_object_set_number(root, "new_int", new_int);
	if (with_match)
		json_object_set_number(root, "match_int", match_int);
	return root;
}

static JSON_Object *build_params_no_new_int(const char *param_name)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, "param_name", param_name);
	return root;
}

static JSON_Object *build_params_no_param_name(void)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_number(root, "new_int", 42.0);
	return root;
}

/* Build a ThreadContext with one named param of given direction
 * and value.
 */
static struct ThreadContext *build_ctx_with_param(const char *name,
						  enum ParamDirections dir,
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
	params[0].param_meta.direction = dir;
	params[0].val = val;

	ctx.params_cnt = 1;
	memcpy(ctx.params, params, sizeof(params));
	ctx.ret_val = 0;
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
	assert(retrace_actions_get("modify_in_param_int") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_int");
	struct ThreadContext *ctx = build_empty_ctx();
	int rc;

	rc = action(ctx, NULL);
	assert(rc == -1);
}

static void test_missing_param_name(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_int");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Object *params = build_params_no_param_name();
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_unknown_param_name(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_int");
	struct ThreadContext *ctx =
		build_ctx_with_param("real_param", PDIR_IN, 0);
	JSON_Object *params =
		build_params_full("nonexistent_param", 99.0, 0, 0.0);
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_param_wrong_direction(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_int");
	struct ThreadContext *ctx =
		build_ctx_with_param("out_param", PDIR_OUT, 0);
	JSON_Object *params =
		build_params_full("out_param", 99.0, 0, 0.0);
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_missing_new_int(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_int");
	struct ThreadContext *ctx =
		build_ctx_with_param("foo", PDIR_IN, 0);
	JSON_Object *params = build_params_no_new_int("foo");
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_direct_modification(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_int");
	struct ThreadContext *ctx =
		build_ctx_with_param("foo", PDIR_IN, 0);
	JSON_Object *params = build_params_full("foo", 42, 0, 0.0);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(ctx->params[0].val == 42);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_match_int_present_and_matches(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_int");
	/* Initial val=10, match_int=10 -> matches, writes new_int=99 */
	struct ThreadContext *ctx =
		build_ctx_with_param("foo", PDIR_IN, 10);
	JSON_Object *params =
		build_params_full("foo", 99.0, 1, 10.0);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(ctx->params[0].val == 99);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_match_int_present_no_match(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_int");
	/* Initial val=10, match_int=20 -> no match, no-op */
	struct ThreadContext *ctx =
		build_ctx_with_param("foo", PDIR_IN, 10);
	JSON_Object *params =
		build_params_full("foo", 99.0, 1, 20.0);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	/* Value unchanged. */
	assert(ctx->params[0].val == 10);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_negative_new_int(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_int");
	struct ThreadContext *ctx =
		build_ctx_with_param("foo", PDIR_IN, 0);
	JSON_Object *params = build_params_full("foo", -7, 0, 0.0);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(ctx->params[0].val == -7);

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

	printf("modify_in_param_int tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_param_name);
	TEST(unknown_param_name);
	TEST(param_wrong_direction);
	TEST(missing_new_int);

	printf("  -- modification semantics --\n");
	TEST(direct_modification);
	TEST(match_int_present_and_matches);
	TEST(match_int_present_no_match);
	TEST(negative_new_int);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
