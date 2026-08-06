/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the modify_in_param_str action.
 *
 * The action swaps a string param's value:
 *   - Allocates a new buffer (free_val=1 so the engine frees it)
 *   - strcpy's new_str into the new buffer
 *   - Optional match_str gates the modification (no match = no-op)
 *
 * Tests:
 *   - Action lookup succeeds
 *   - NULL params -> -1
 *   - Missing param_name -> -1
 *   - Unknown param name -> -1
 *   - Missing new_str -> -1
 *   - Direct modification (no match_str)
 *   - match_str present and matches -> writes new value
 *   - match_str present, no match -> no-op
 *   - Repeated modification frees the prior buffer (free_val flag)
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

static JSON_Object *build_params_full(const char *param_name,
				       const char *new_str,
				       int with_match,
				       const char *match_str)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, "param_name", param_name);
	json_object_set_string(root, "new_str", new_str);
	if (with_match)
		json_object_set_string(root, "match_str", match_str);
	return root;
}

static JSON_Object *build_params_no_new_str(const char *param_name)
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

	json_object_set_string(root, "new_str", "ignored");
	return root;
}

static struct ThreadContext *build_ctx_with_str_param(const char *name,
						       const char *val)
{
	static struct ThreadContext ctx;
	static struct FuncPrototype proto;
	static char buf[256];
	struct FuncParam params[8];

	memset(&ctx, 0, sizeof(ctx));
	memset(params, 0, sizeof(params));
	memset(&proto, 0, sizeof(proto));

	strncpy(proto.name, "test_func", sizeof(proto.name) - 1);
	ctx.prototype = &proto;

	strncpy(buf, val, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	strncpy(params[0].param_meta.name, name,
		sizeof(params[0].param_meta.name) - 1);
	params[0].param_meta.modifiers = CDM_POINTER;
	params[0].param_meta.direction = PDIR_IN;
	strcpy(params[0].param_meta.ref_type_name, "sz");
	params[0].val = (long)buf;
	params[0].free_val = 0;

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
	assert(retrace_actions_get("modify_in_param_str") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_str");
	struct ThreadContext *ctx = build_empty_ctx();
	int rc;

	rc = action(ctx, NULL);
	assert(rc == -1);
}

static void test_missing_param_name(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_str");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Object *params = build_params_no_param_name();
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_unknown_param_name(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_str");
	struct ThreadContext *ctx =
		build_ctx_with_str_param("real_param", "value");
	JSON_Object *params =
		build_params_full("nonexistent", "new", 0, NULL);
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_missing_new_str(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_str");
	struct ThreadContext *ctx =
		build_ctx_with_str_param("foo", "value");
	JSON_Object *params = build_params_no_new_str("foo");
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_direct_modification(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_str");
	struct ThreadContext *ctx =
		build_ctx_with_str_param("foo", "original");
	JSON_Object *params =
		build_params_full("foo", "replacement", 0, NULL);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(strcmp((char *)ctx->params[0].val, "replacement") == 0);
	assert(ctx->params[0].free_val == 1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_match_str_present_and_matches(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_str");
	struct ThreadContext *ctx =
		build_ctx_with_str_param("foo", "old_value");
	JSON_Object *params =
		build_params_full("foo", "new_value", 1, "old_value");
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(strcmp((char *)ctx->params[0].val, "new_value") == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_match_str_present_no_match(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_str");
	struct ThreadContext *ctx =
		build_ctx_with_str_param("foo", "actual_value");
	JSON_Object *params =
		build_params_full("foo", "new_value", 1, "different_value");
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	/* Original value unchanged. */
	assert(strcmp((char *)ctx->params[0].val, "actual_value") == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_repeated_modification_frees_prior_buffer(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_str");
	struct ThreadContext *ctx =
		build_ctx_with_str_param("foo", "initial");
	JSON_Object *p1 = build_params_full("foo", "first", 0, NULL);
	JSON_Object *p2 = build_params_full("foo", "second", 0, NULL);
	int rc;

	/* First modification allocates a new buffer; free_val=1. */
	rc = action(ctx, p1);
	assert(rc == 0);
	assert(ctx->params[0].free_val == 1);
	assert(strcmp((char *)ctx->params[0].val, "first") == 0);

	/* Second modification should free the first buffer before
	 * allocating the second.
	 */
	rc = action(ctx, p2);
	assert(rc == 0);
	assert(ctx->params[0].free_val == 1);
	assert(strcmp((char *)ctx->params[0].val, "second") == 0);

	/* Manual cleanup: the engine would free this. */
	free((void *)ctx->params[0].val);

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

	printf("modify_in_param_str tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_param_name);
	TEST(unknown_param_name);
	TEST(missing_new_str);

	printf("  -- modification semantics --\n");
	TEST(direct_modification);
	TEST(match_str_present_and_matches);
	TEST(match_str_present_no_match);
	TEST(repeated_modification_frees_prior_buffer);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
