/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the sandbox action (file-path policy).
 *
 * The action is the file-system counterpart of addr_deny (PR #553):
 * it finds the first string param (prototype metadata: CDM_POINTER
 * with ref "sz") among the first two args, compares it against the
 * deny_paths / allow_paths lists, and on a hit sets ret_val=-1,
 * errno=EACCES and returns -1.
 *
 * allow_paths is the jail mode (TODO.windows/08): deny-by-default,
 * emitted by retrace-profile --jail-out.
 *
 * Tests:
 *   - Action lookup succeeds
 *   - Required params validation (NULL params, missing lists)
 *   - Exact-match deny / prefix-match deny (also '\' separator)
 *   - No-match passes through
 *   - allow_paths: deny-by-default, listed path passes
 *   - deny + allow combined: a path must pass both
 *   - Non-string params (close(fd)) are never treated as paths
 *
 * Part of TODO.complete/14 and TODO.windows/08.
 */

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

/* Always-on check: CHECK() compiles to nothing under NDEBUG. */
#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
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

/*
 * One param descriptor: name, value, and whether it is a string
 * pointer (like open's path) or a plain int (like close's fd).
 */
struct param_desc {
	const char *name;
	const char *str_val;
	long int_val;
	int is_string;
};

/* Build a ThreadContext with up to 2 params (see param_desc). */
static struct ThreadContext *build_ctx(const struct param_desc *p, int n)
{
	static struct ThreadContext ctx;
	static char bufs[2][256];
	int i;

	memset(&ctx, 0, sizeof(ctx));
	ctx.params_cnt = n;
	for (i = 0; i < n && i < 2; i++) {
		strncpy(ctx.params[i].param_meta.name, p[i].name,
			sizeof(ctx.params[i].param_meta.name) - 1);
		if (p[i].is_string) {
			strncpy(bufs[i], p[i].str_val, sizeof(bufs[i]) - 1);
			bufs[i][sizeof(bufs[i]) - 1] = '\0';
			ctx.params[i].val = (long)bufs[i];
			ctx.params[i].param_meta.modifiers = CDM_POINTER;
			strncpy(ctx.params[i].param_meta.ref_type_name,
				"sz",
				sizeof(ctx.params[i].param_meta
					.ref_type_name) - 1);
		} else {
			ctx.params[i].val = p[i].int_val;
			ctx.params[i].param_meta.modifiers = CDM_NOMOD;
		}
	}
	ctx.ret_val = 0;
	return &ctx;
}

static struct ThreadContext *build_empty_ctx(void)
{
	static struct ThreadContext ctx;

	memset(&ctx, 0, sizeof(ctx));
	return &ctx;
}

static void free_params(JSON_Object *params)
{
	json_value_free(json_object_get_wrapping_value(params));
}

static void test_action_lookup(void)
{
	retrace_actions_init();
	CHECK(retrace_actions_get("sandbox") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	struct ThreadContext *ctx = build_empty_ctx();
	int rc;

	rc = action(ctx, NULL);
	CHECK(rc == -1);
	CHECK(ctx->ret_val == -1);
}

static void test_missing_lists(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	struct ThreadContext *ctx = build_empty_ctx();
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	struct param_desc pd = { "path", "/etc/shadow", 0, 1 };
	struct ThreadContext *pctx;
	int rc;

	json_object_set_string(root, "unrelated", "foo");
	pctx = build_ctx(&pd, 1);
	rc = action(pctx, root);
	CHECK(rc == -1);
	CHECK(pctx->ret_val == -1);

	json_value_free(root_val);
}

static void test_exact_match(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/etc/shadow" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 1);
	struct param_desc pd = { "path", "/etc/shadow", 0, 1 };
	struct ThreadContext *ctx = build_ctx(&pd, 1);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == -1);
	CHECK(ctx->ret_val == -1);

	free_params(params);
}

static void test_no_match_passes_through(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/etc/shadow" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 1);
	struct param_desc pd = { "path", "/tmp/safe.txt", 0, 1 };
	struct ThreadContext *ctx = build_ctx(&pd, 1);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == 0);
	CHECK(ctx->ret_val == 0);

	free_params(params);
}

static void test_prefix_match_directory(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/root/" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 1);
	struct param_desc pd = { "path", "/root/.ssh/id_rsa", 0, 1 };
	struct ThreadContext *ctx = build_ctx(&pd, 1);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == -1);
	CHECK(ctx->ret_val == -1);

	free_params(params);
}

static void test_prefix_backslash_separator(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "C:\\Users\\secret\\" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 1);
	struct param_desc pd = { "path", "C:\\Users\\secret\\k.dat", 0, 1 };
	struct ThreadContext *ctx = build_ctx(&pd, 1);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == -1);

	free_params(params);
}

static void test_prefix_no_match_different_dir(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/root/" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 1);
	struct param_desc pd = { "path", "/etc/passwd", 0, 1 };
	struct ThreadContext *ctx = build_ctx(&pd, 1);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == 0);

	free_params(params);
}

static void test_openat_picks_string_param(void)
{
	/* openat(dirfd, path, flags): param[0] is an int fd, param[1]
	 * is the path. The prototype-metadata gate must skip the fd
	 * and police the string.
	 */
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/etc/shadow" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 1);
	struct param_desc pd[2] = {
		{ "dirfd", NULL, 3, 0 },
		{ "path", "/etc/shadow", 0, 1 }
	};
	struct ThreadContext *ctx = build_ctx(pd, 2);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == -1);
	CHECK(ctx->ret_val == -1);

	free_params(params);
}

static void test_int_param_never_a_path(void)
{
	/* close(fd): no string param at all -- even in
	 * deny-by-default allow mode the call passes through.
	 */
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/only/allowed.dat" };
	JSON_Object *params =
		build_params_with_array("allow_paths", paths, 1);
	struct param_desc pd = { "fd", NULL, 7, 0 };
	struct ThreadContext *ctx = build_ctx(&pd, 1);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == 0);
	CHECK(ctx->ret_val == 0);

	free_params(params);
}

static void test_first_match_wins(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/safe", "/etc/shadow", "/other" };
	JSON_Object *params =
		build_params_with_array("deny_paths", paths, 3);
	struct param_desc pd = { "path", "/etc/shadow", 0, 1 };
	struct ThreadContext *ctx = build_ctx(&pd, 1);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == -1);
	CHECK(ctx->ret_val == -1);

	free_params(params);
}

static void test_allow_denies_unlisted(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/vfs/entry.dat" };
	JSON_Object *params =
		build_params_with_array("allow_paths", paths, 1);
	struct param_desc pd = { "path", "/vfs/leaked.dat", 0, 1 };
	struct ThreadContext *ctx = build_ctx(&pd, 1);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == -1);
	CHECK(ctx->ret_val == -1);

	free_params(params);
}

static void test_allow_passes_listed(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/vfs/entry.dat", "/vfs/settings.dat" };
	JSON_Object *params =
		build_params_with_array("allow_paths", paths, 2);
	struct param_desc pd = { "path", "/vfs/settings.dat", 0, 1 };
	struct ThreadContext *ctx = build_ctx(&pd, 1);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == 0);
	CHECK(ctx->ret_val == 0);

	free_params(params);
}

static void test_allow_prefix_entry(void)
{
	action_fn_t action = retrace_actions_get("sandbox");
	const char *paths[] = { "/vfs/" };
	JSON_Object *params =
		build_params_with_array("allow_paths", paths, 1);
	struct param_desc pd = { "path", "/vfs/sub/deep/file.dat", 0, 1 };
	struct ThreadContext *ctx = build_ctx(&pd, 1);
	int rc;

	rc = action(ctx, params);
	CHECK(rc == 0);

	free_params(params);
}

static void test_deny_wins_over_allow(void)
{
	/* Both lists: allowed by allow_paths but listed in
	 * deny_paths -- deny wins.
	 */
	action_fn_t action = retrace_actions_get("sandbox");
	const char *allow[] = { "/vfs/entry.dat" };
	const char *deny[] = { "/vfs/entry.dat" };
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	JSON_Value *av = json_value_init_array();
	JSON_Value *dv = json_value_init_array();
	struct param_desc pd = { "path", "/vfs/entry.dat", 0, 1 };
	struct ThreadContext *ctx = build_ctx(&pd, 1);
	int rc;

	json_array_append_string(json_array(av), allow[0]);
	json_array_append_string(json_array(dv), deny[0]);
	json_object_set_value(root, "allow_paths", av);
	json_object_set_value(root, "deny_paths", dv);

	rc = action(ctx, root);
	CHECK(rc == -1);
	CHECK(ctx->ret_val == -1);

	json_value_free(root_val);
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
	TEST(missing_lists);

	printf("  -- deny_paths matching --\n");
	TEST(exact_match);
	TEST(prefix_match_directory);
	TEST(prefix_backslash_separator);
	TEST(prefix_no_match_different_dir);
	TEST(first_match_wins);

	printf("  -- param selection --\n");
	TEST(openat_picks_string_param);
	TEST(int_param_never_a_path);

	printf("  -- allow_paths (jail mode) --\n");
	TEST(allow_denies_unlisted);
	TEST(allow_passes_listed);
	TEST(allow_prefix_entry);
	TEST(deny_wins_over_allow);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
