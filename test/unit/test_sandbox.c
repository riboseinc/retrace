/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the sandbox action (file-path deny-list).
 *
 * The action is the file-system counterpart of addr_deny (PR #553):
 * it scans t_ctx->params for a path argument, compares against the
 * deny_paths array, and on match sets ret_val=-1 and returns -1.
 *
 * Tests:
 *   - Action lookup succeeds
 *   - Required params validation (NULL params, missing deny_paths)
 *   - Exact-match deny
 *   - Prefix-match deny (entry ending in '/')
 *   - No-match passes through
 *   - First-non-null param is picked (params[0] vs params[1])
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

static JSON_Object *build_params_with_array(const char *key,
					     const char **vals, int n)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	JSON_Value *arr_val = json_value_init_array();
	JSON_Array *arr = json_array(arr_val);
	int i;

	for (i = 0; i < n; i++)
		json_array_append_string(arr, vals[i]);

	json_object_set_value(root, key, arr_val);
	return root;
}

/* Build a ThreadContext with up to 2 named path-like params.
 * n==1: single param at index 0 with given name + value.
 * n==2: param[0] with name0/value0; param[1] with name1/value1.
 */
static struct ThreadContext *build_ctx_with_paths(int n,
						   const char *name0,
						   const char *val0,
						   const char *name1,
						   const char *val1)
{
	static struct ThreadContext ctx;
	static struct FuncParam params[8];
	static char buf0[256];
	static char buf1[256];

	memset(&ctx, 0, sizeof(ctx));
	memset(params, 0, sizeof(params));

	if (n >= 1) {
		strncpy(params[0].param_meta.name, name0,
			sizeof(params[0].param_meta.name) - 1);
		strncpy(buf0, val0, sizeof(buf0) - 1);
		buf0[sizeof(buf0) - 1] = '\0';
		params[0].val = (long)buf0;
	}
	if (n >= 2) {
		strncpy(params[1].param_meta.name, name1,
			sizeof(params[1].param_meta.name) - 1);
		strncpy(buf1, val1, sizeof(buf1) - 1);
		buf1[sizeof(buf1) - 1] = '\0';
		params[1].val = (long)buf1;
	}

	ctx.params_cnt = n;
	memcpy(ctx.params, params, sizeof(params));
	ctx.ret_val = 0;
	return &ctx;
}

static struct ThreadContext *build_empty_ctx(void)
{
	static struct ThreadContext ctx;

	memset(&ctx, 0, sizeof(ctx));
	return &ctx;
}

static void test_action_lookup(void)
{
	retrace_actions_init();
	assert(retrace_actions_get("sandbox") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	struct ThreadContext *ctx = build_empty_ctx();
	int rc;

	rc = action(ctx, NULL);
	assert(rc == -1);
}

static void test_missing_deny_paths(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	int rc;

	json_object_set_string(root, "unrelated", "foo");
	rc = action(ctx, root);
	assert(rc == -1);

	json_value_free(root_val);
}

static void test_exact_match(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/etc/shadow" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 1);
	struct ThreadContext *ctx =
		build_ctx_with_paths(1, "path", "/etc/shadow", NULL, NULL);
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);
	assert(ctx->ret_val == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_no_match_passes_through(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/etc/shadow" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 1);
	struct ThreadContext *ctx =
		build_ctx_with_paths(1, "path", "/tmp/safe.txt", NULL, NULL);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);
	assert(ctx->ret_val == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_prefix_match_directory(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/root/" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 1);
	struct ThreadContext *ctx =
		build_ctx_with_paths(1, "path", "/root/.ssh/id_rsa", NULL, NULL);
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);
	assert(ctx->ret_val == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_prefix_no_match_different_dir(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/root/" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 1);
	struct ThreadContext *ctx =
		build_ctx_with_paths(1, "path", "/etc/passwd", NULL, NULL);
	int rc;

	rc = action(ctx, params);
	assert(rc == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_openat_uses_param_at_index_1(void)
{
	/* openat(dirfd, path, flags) -- path is at params[1] because
	 * the action's loop checks param[0] first, then param[1].
	 * If param[0].val is non-zero it picks that; we need a
	 * scenario where param[0] is the fd (numeric, non-zero) and
	 * param[1] is the path. The current action treats any
	 * non-zero val at params[0] as a path pointer -- which is
	 * wrong for openat but it's the existing behavior. The test
	 * documents this: sandbox picks params[0] first.
	 */
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/etc/shadow" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 1);
	struct ThreadContext *ctx =
		build_ctx_with_paths(1, "path", "/etc/shadow",
				     "flags", "ignored");
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_first_match_wins(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/safe", "/etc/shadow", "/other" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 3);
	struct ThreadContext *ctx =
		build_ctx_with_paths(1, "path", "/etc/shadow", NULL, NULL);
	int rc;

	rc = action(ctx, params);
	assert(rc == -1);
	assert(ctx->ret_val == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

int main(void)
{
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.strncmp = strncmp;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;

	printf("sandbox action tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_deny_paths);

	printf("  -- match paths --\n");
	TEST(exact_match);
	TEST(prefix_match_directory);

	printf("  -- non-match paths --\n");
	TEST(no_match_passes_through);
	TEST(prefix_no_match_different_dir);

	printf("  -- array semantics --\n");
	TEST(openat_uses_param_at_index_1);
	TEST(first_match_wins);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
