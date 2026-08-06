/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the log_params action.
 *
 * log_params is the most-used action (the default config runs it
 * for every intercepted call). It walks each param, looks up its
 * DataType, and serializes the value to a JSON log entry.
 *
 * The action's contract:
 *   - Returns 0 on success
 *   - For each param, param->data_type must be non-NULL (set by
 *     the engine from the prototype metadata)
 *   - omit_params (optional JSON array) skips named params
 *
 * Tests:
 *   - Action lookup succeeds
 *   - Single int param: doesn't crash, returns 0
 *   - Multiple int params: all walked without crash
 *   - omit_params excludes a named param (still returns 0)
 *   - NULL params arg: still walks the ThreadContext
 *   - Zero-param ctx: returns 0 immediately
 *
 * Note: the JSON output is logged via retrace_logger_log_json,
 * which goes to stdout/file. The test asserts only the return
 * code and absence of crash. Capturing the JSON output would
 * require log redirection plumbing (deferred to a future test
 * infrastructure PR).
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

/* Build a ThreadContext with N integer params of the given values.
 * Each param's data_type is set to the registered "int" DataType.
 */
static struct ThreadContext *build_ctx_int_params(int n, const long *vals)
{
	static struct ThreadContext ctx;
	static struct FuncPrototype proto;
	struct FuncParam params[8];
	const struct DataType *int_dt;
	int i;

	memset(&ctx, 0, sizeof(ctx));
	memset(params, 0, sizeof(params));
	memset(&proto, 0, sizeof(proto));

	int_dt = retrace_datatype_get("int");
	assert(int_dt != NULL);

	strncpy(proto.name, "test_func", sizeof(proto.name) - 1);
	proto.fmt = FAT_NOVARARGS;
	ctx.prototype = &proto;

	for (i = 0; i < n && i < 8; i++) {
		char nm[4];

		snprintf(nm, sizeof(nm), "p%d", i);
		strncpy(params[i].param_meta.name, nm,
			sizeof(params[i].param_meta.name) - 1);
		params[i].param_meta.type_name[0] = 'i';
		params[i].param_meta.type_name[1] = 'n';
		params[i].param_meta.type_name[2] = 't';
		params[i].param_meta.type_name[3] = '\0';
		params[i].param_meta.modifiers = CDM_NOMOD;
		params[i].param_meta.direction = PDIR_IN;
		params[i].data_type = int_dt;
		params[i].val = vals[i];
	}

	ctx.params_cnt = n;
	memcpy(ctx.params, params, sizeof(params));
	ctx.ret_val = 0;
	return &ctx;
}

static JSON_Object *build_omit_params(const char *name)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	JSON_Value *arr_val = json_value_init_array();
	JSON_Array *arr = json_array(arr_val);

	json_array_append_string(arr, name);
	json_object_set_value(root, "omit_params", arr_val);
	return root;
}

static void test_action_lookup(void)
{
	retrace_actions_init();
	retrace_datatypes_init();
	assert(retrace_actions_get("log_params") != NULL);
}

static void test_zero_params(void)
{
	action_fn_t action = retrace_actions_get("log_params");
	struct ThreadContext ctx;
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	ctx.params_cnt = 0;

	rc = action(&ctx, NULL);
	assert(rc == 0);
}

static void test_single_int_param(void)
{
	action_fn_t action = retrace_actions_get("log_params");
	long vals[] = { 42 };
	struct ThreadContext *ctx = build_ctx_int_params(1, vals);
	int rc;

	rc = action(ctx, NULL);
	assert(rc == 0);
}

static void test_multiple_int_params(void)
{
	action_fn_t action = retrace_actions_get("log_params");
	long vals[] = { 1, 2, 3, 4 };
	struct ThreadContext *ctx = build_ctx_int_params(4, vals);
	int rc;

	rc = action(ctx, NULL);
	assert(rc == 0);
}

static void test_omit_one_param(void)
{
	action_fn_t action = retrace_actions_get("log_params");
	long vals[] = { 10, 20, 30 };
	struct ThreadContext *ctx = build_ctx_int_params(3, vals);
	JSON_Object *params = build_omit_params("p1");
	int rc;

	/* omit p1, walk p0 and p2. */
	rc = action(ctx, params);
	assert(rc == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_null_params_arg(void)
{
	action_fn_t action = retrace_actions_get("log_params");
	long vals[] = { 7 };
	struct ThreadContext *ctx = build_ctx_int_params(1, vals);
	int rc;

	/* NULL action_params is allowed (omit_params defaults to NULL). */
	rc = action(ctx, NULL);
	assert(rc == 0);
}

static void test_negative_int_param(void)
{
	action_fn_t action = retrace_actions_get("log_params");
	long vals[] = { -123 };
	struct ThreadContext *ctx = build_ctx_int_params(1, vals);
	int rc;

	rc = action(ctx, NULL);
	assert(rc == 0);
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
	retrace_real_impls.real_sprintf = sprintf;
	retrace_real_impls.real_vsnprintf = vsnprintf;

	printf("log_params tests:\n");

	printf("  -- lookup + zero-param --\n");
	TEST(action_lookup);
	TEST(zero_params);

	printf("  -- int param walks --\n");
	TEST(single_int_param);
	TEST(multiple_int_params);
	TEST(negative_int_param);

	printf("  -- omit_params --\n");
	TEST(omit_one_param);

	printf("  -- NULL action_params --\n");
	TEST(null_params_arg);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
