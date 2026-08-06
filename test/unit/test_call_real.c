/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the call_real action.
 *
 * call_real invokes t_ctx->real_impl via retrace_as_call_real /
 * retrace_as_call_real_variadic. It also measures call duration
 * via clock_gettime and logs the timing as a JSON entry.
 *
 * The test installs a stub function as real_impl so we can verify
 * the call mechanics without depending on libc internals.
 *
 * Tests:
 *   - Action lookup succeeds
 *   - Zero-arg stub: called exactly once; ret_val matches stub return
 *   - One-arg stub: arg comes from params[0].val
 *   - Action returns 0 (success) on every path
 *
 * Note: the timing JSON is logged via retrace_logger_log_json; the
 * test does not capture it. The clock_gettime call itself is the
 * only libc dependency, which is always available.
 *
 * Part of TODO.complete/14.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "engine.h"
#include "actions.h"
#include "arch_spec.h"
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

/* Stub functions used as real_impl. Their signatures match what
 * retrace_as_call_real_dispatch expects for each params_cnt case.
 */
static long g_zero_arg_call_count;
static long g_one_arg_last_val;

static long stub_zero_arg(void)
{
	g_zero_arg_call_count++;
	return 0x4242;
}

static long stub_one_arg(long a)
{
	g_one_arg_last_val = a;
	return a + 1;
}

static struct ThreadContext *build_ctx_for_call_real(void *real_impl,
						     int params_cnt,
						     const long *vals)
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

	strncpy(proto.name, "stub_func", sizeof(proto.name) - 1);
	proto.fmt = FAT_NOVARARGS;
	ctx.prototype = &proto;
	ctx.real_impl = real_impl;

	for (i = 0; i < params_cnt && i < 8; i++) {
		snprintf(params[i].param_meta.name,
			sizeof(params[i].param_meta.name), "p%d", i);
		params[i].param_meta.modifiers = CDM_NOMOD;
		params[i].param_meta.direction = PDIR_IN;
		params[i].data_type = int_dt;
		params[i].val = vals[i];
	}

	ctx.params_cnt = params_cnt;
	memcpy(ctx.params, params, sizeof(params));
	ctx.ret_val = 0;
	return &ctx;
}

static void test_action_lookup(void)
{
	retrace_actions_init();
	retrace_datatypes_init();
	assert(retrace_actions_get("call_real") != NULL);
}

static void test_zero_arg_stub(void)
{
	action_fn_t action = retrace_actions_get("call_real");
	struct ThreadContext *ctx;
	int rc;

	g_zero_arg_call_count = 0;
	ctx = build_ctx_for_call_real(stub_zero_arg, 0, NULL);

	rc = action(ctx, NULL);
	assert(rc == 0);
	assert(g_zero_arg_call_count == 1);
	/* retrace_as_call_real_dispatch returns the stub's return value;
	 * call_real stores it in t_ctx->ret_val.
	 */
	assert(ctx->ret_val == 0x4242);
}

static void test_one_arg_stub(void)
{
	action_fn_t action = retrace_actions_get("call_real");
	long vals[] = { 99 };
	struct ThreadContext *ctx;
	int rc;

	g_one_arg_last_val = -1;
	ctx = build_ctx_for_call_real(stub_one_arg, 1, vals);

	rc = action(ctx, NULL);
	assert(rc == 0);
	assert(g_one_arg_last_val == 99);
	/* stub returns a+1 = 100. */
	assert(ctx->ret_val == 100);
}

static void test_action_params_ignored(void)
{
	action_fn_t action = retrace_actions_get("call_real");
	struct ThreadContext *ctx;
	JSON_Value *root_val;
	JSON_Object *root;
	int rc;

	/* action_params is ignored by call_real. Pass a non-NULL object
	 * to verify it doesn't cause issues.
	 */
	root_val = json_value_init_object();
	root = json_value_get_object(root_val);
	json_object_set_string(root, "anything", "value");

	g_zero_arg_call_count = 0;
	ctx = build_ctx_for_call_real(stub_zero_arg, 0, NULL);

	rc = action(ctx, root);
	assert(rc == 0);
	assert(g_zero_arg_call_count == 1);

	json_value_free(root_val);
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

	printf("call_real tests:\n");

	printf("  -- lookup --\n");
	TEST(action_lookup);

	printf("  -- stub invocations --\n");
	TEST(zero_arg_stub);
	TEST(one_arg_stub);

	printf("  -- action_params handling --\n");
	TEST(action_params_ignored);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
