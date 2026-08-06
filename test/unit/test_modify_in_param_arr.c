/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the modify_in_param_arr action.
 *
 * The action swaps an array param's contents:
 *   - Looks up param by name (must be PDIR_IN + CDM_POINTER)
 *   - Reads new_arr (JSON array of bytes)
 *   - Optional match_arr (JSON array of bytes) gates: must equal
 *     current param contents byte-for-byte (no match = no-op)
 *   - Allocates new buffer (free_val=1), fills with new_arr bytes
 *   - If param has array_cnt_param set, updates the count param
 *
 * Tests:
 *   - Action lookup succeeds
 *   - NULL params -> -1
 *   - Missing param_name -> -1
 *   - Unknown param name -> -1
 *   - Wrong direction (PDIR_OUT) -> -1
 *   - Not a pointer (no CDM_POINTER) -> -1
 *   - Missing new_arr -> -1
 *   - Empty new_arr -> -1
 *   - Direct modification (no match_arr)
 *   - match_arr present and matches -> writes
 *   - match_arr present, no match -> no-op
 *
 * Part of TODO.complete/14. Completes the action-test sweep:
 * with this, all 14 built-in actions have unit-test coverage.
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

/* Build params object with required param_name + new_arr (bytes).
 * Optional match_arr (bytes). Bytes are JSON numbers 0..255.
 */
static JSON_Object *build_params_arr(const char *param_name,
				     const int *new_bytes, int new_n,
				     int with_match,
				     const int *match_bytes, int match_n)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	JSON_Value *new_arr_val = json_value_init_array();
	JSON_Array *new_arr = json_array(new_arr_val);
	int i;

	for (i = 0; i < new_n; i++)
		json_array_append_number(new_arr, new_bytes[i]);
	json_object_set_value(root, "param_name",
		json_value_init_string(param_name));
	json_object_set_value(root, "new_arr", new_arr_val);

	if (with_match) {
		JSON_Value *match_arr_val = json_value_init_array();
		JSON_Array *match_arr = json_array(match_arr_val);

		for (i = 0; i < match_n; i++)
			json_array_append_number(match_arr, match_bytes[i]);
		json_object_set_value(root, "match_arr", match_arr_val);
	}

	return root;
}

static JSON_Object *build_params_no_new_arr(const char *param_name)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, "param_name", param_name);
	return root;
}

static JSON_Object *build_params_empty_new_arr(const char *param_name)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	JSON_Value *arr_val = json_value_init_array();

	json_object_set_string(root, "param_name", param_name);
	json_object_set_value(root, "new_arr", arr_val);
	return root;
}

/* Build a ThreadContext with one array param. caller_buf is the
 * initial buffer the param points to (must remain valid for the
 * test's lifetime).
 */
static struct ThreadContext *build_ctx_with_arr_param(const char *name,
						       unsigned char *caller_buf,
						       int buf_len,
						       enum ParamDirections dir,
						       int is_pointer)
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
	if (is_pointer)
		params[0].param_meta.modifiers = CDM_POINTER;
	else
		params[0].param_meta.modifiers = CDM_NOMOD;
	params[0].val = (long)caller_buf;
	params[0].free_val = 0;

	ctx.params_cnt = 1;
	memcpy(ctx.params, params, sizeof(params));
	ctx.ret_val = 0;
	(void)buf_len;
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
	assert(retrace_actions_get("modify_in_param_arr") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_arr");
	struct ThreadContext *ctx = build_empty_ctx();
	int rc;

	rc = action(ctx, NULL);
	assert(rc == -1);
}

static void test_missing_param_name(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_arr");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	int rc;

	json_object_set_string(root, "unrelated", "foo");
	rc = action(ctx, root);
	assert(rc == -1);

	json_value_free(root_val);
}

static void test_unknown_param_name(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_arr");
	unsigned char buf[4] = { 0, 0, 0, 0 };
	struct ThreadContext *ctx =
		build_ctx_with_arr_param("real", buf, 4, PDIR_IN, 1);
	const int new_bytes[] = { 1, 2, 3 };
	JSON_Object *params =
		build_params_arr("nonexistent", new_bytes, 3, 0, NULL, 0);
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_wrong_direction(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_arr");
	unsigned char buf[4] = { 0, 0, 0, 0 };
	struct ThreadContext *ctx =
		build_ctx_with_arr_param("arr", buf, 4, PDIR_OUT, 1);
	const int new_bytes[] = { 1, 2 };
	JSON_Object *params =
		build_params_arr("arr", new_bytes, 2, 0, NULL, 0);
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_not_a_pointer(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_arr");
	unsigned char buf[4] = { 0, 0, 0, 0 };
	struct ThreadContext *ctx =
		build_ctx_with_arr_param("arr", buf, 4, PDIR_IN, 0);
	const int new_bytes[] = { 1, 2 };
	JSON_Object *params =
		build_params_arr("arr", new_bytes, 2, 0, NULL, 0);
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_missing_new_arr(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_arr");
	unsigned char buf[4] = { 0, 0, 0, 0 };
	struct ThreadContext *ctx =
		build_ctx_with_arr_param("arr", buf, 4, PDIR_IN, 1);
	JSON_Object *params = build_params_no_new_arr("arr");
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_empty_new_arr(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_arr");
	unsigned char buf[4] = { 0, 0, 0, 0 };
	struct ThreadContext *ctx =
		build_ctx_with_arr_param("arr", buf, 4, PDIR_IN, 1);
	JSON_Object *params = build_params_empty_new_arr("arr");
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_direct_modification(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_arr");
	unsigned char buf[4] = { 9, 9, 9, 9 };
	struct ThreadContext *ctx =
		build_ctx_with_arr_param("arr", buf, 4, PDIR_IN, 1);
	const int new_bytes[] = { 0xDE, 0xAD, 0xBE, 0xEF };
	JSON_Object *params =
		build_params_arr("arr", new_bytes, 4, 0, NULL, 0);
	unsigned char *out;
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);

	out = (unsigned char *)ctx->params[0].val;
	assert(out[0] == 0xDE);
	assert(out[1] == 0xAD);
	assert(out[2] == 0xBE);
	assert(out[3] == 0xEF);
	assert(ctx->params[0].free_val == 1);

	/* Manual cleanup: engine would do this via thread_context_clear. */
	free(out);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_match_arr_present_and_matches(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_arr");
	unsigned char buf[3] = { 0xAA, 0xBB, 0xCC };
	struct ThreadContext *ctx =
		build_ctx_with_arr_param("arr", buf, 3, PDIR_IN, 1);
	const int match_bytes[] = { 0xAA, 0xBB, 0xCC };
	const int new_bytes[] = { 0x11, 0x22 };
	JSON_Object *params =
		build_params_arr("arr", new_bytes, 2, 1, match_bytes, 3);
	unsigned char *out;
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);

	out = (unsigned char *)ctx->params[0].val;
	assert(out[0] == 0x11);
	assert(out[1] == 0x22);

	free(out);
	json_value_free(json_object_get_wrapping_value(params));
}

static void test_match_arr_present_no_match(void)
{
	action_fn_t action = retrace_actions_get("modify_in_param_arr");
	unsigned char buf[3] = { 0xAA, 0xBB, 0xCC };
	struct ThreadContext *ctx =
		build_ctx_with_arr_param("arr", buf, 3, PDIR_IN, 1);
	const int match_bytes[] = { 0xFF, 0xBB, 0xCC };  /* first byte differs */
	const int new_bytes[] = { 0x11, 0x22 };
	JSON_Object *params =
		build_params_arr("arr", new_bytes, 2, 1, match_bytes, 3);
	int rc;

	rc = action(ctx, params);
	/* No-match returns 0 (success, no-op). */
	assert(rc == 0);
	/* Original buffer pointer unchanged, free_val still 0. */
	assert(ctx->params[0].free_val == 0);

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

	printf("modify_in_param_arr tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_param_name);
	TEST(unknown_param_name);
	TEST(wrong_direction);
	TEST(not_a_pointer);
	TEST(missing_new_arr);
	TEST(empty_new_arr);

	printf("  -- modification semantics --\n");
	TEST(direct_modification);
	TEST(match_arr_present_and_matches);
	TEST(match_arr_present_no_match);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
