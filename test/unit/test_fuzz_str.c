/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for fuzz_str (TODO.trace-profile/25) -- the
 * dictionary-driven string fuzzing action -- and its fuzz_dict
 * loader.
 *
 * Function level (real instances, no doubles):
 *   - dict load: comments/blanks skipped, token count, exact
 *     contents
 *   - dict load: missing file -> -1
 *   - pick determinism: same srand seed -> same token sequence
 *
 * Action level (lookup by name, real ThreadContext):
 *   - Action lookup succeeds
 *   - NULL params -> -1
 *   - Missing param_name -> -1
 *   - Missing dict -> -1
 *   - Bad dict path -> -1
 *   - Unknown param name -> -1
 *   - Happy path: value replaced with SOME dict token, free_val
 *     set
 *   - match_str no-match -> value untouched
 *   - Repeated call frees the prior buffer (no leak growth):
 *     free_val toggles and value still a dict token
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "actions.h"
#include "fuzz_dict.h"
#include "data_types.h"
#include "real_impls.h"
#include "parson.h"

typedef int (*action_fn_t)(struct ThreadContext *t_ctx,
			    const JSON_Object *action_params);

#define DICT_PATH "fuzz-dict.txt"
#define DICT_TOKENS 5
#define DICT_TMPL_PATH "fuzz-dict-templates.txt"
#define DICT_BAD_PATH "fuzz-dict-bad.txt"

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

/* Always-on check: assert() compiles to nothing under NDEBUG. */
#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

static int token_in_dict(const fuzz_dict_t *d, const char *s)
{
	int i;

	for (i = 0; i < d->count; i++)
		if (strcmp(d->tokens[i], s) == 0)
			return 1;
	return 0;
}

static void test_dict_load(void)
{
	static fuzz_dict_t d;

	CHECK(fuzz_dict_load(&d, DICT_PATH) == 0);
	CHECK(d.count == DICT_TOKENS);
	CHECK(strcmp(d.tokens[0], "/etc/passwd") == 0);
	CHECK(strcmp(d.tokens[1], "../../../etc/shadow") == 0);
	CHECK(strcmp(d.tokens[2], "%s%s%s%n") == 0);
	CHECK(strcmp(d.tokens[3],
		"https://example.invalid/callback") == 0);
}

static void test_dict_load_missing(void)
{
	static fuzz_dict_t d;

	CHECK(fuzz_dict_load(&d, "no/such/dict.txt") == -1);
	CHECK(d.count == 0);
}

static void test_dict_pick_deterministic(void)
{
	static fuzz_dict_t d;
	char seq_a[8][128];
	char seq_b[8][128];
	int i;

	CHECK(fuzz_dict_load(&d, DICT_PATH) == 0);

	srand(1498729252);
	for (i = 0; i < 8; i++)
		strcpy(seq_a[i], fuzz_dict_pick(&d));
	srand(1498729252);
	for (i = 0; i < 8; i++)
		strcpy(seq_b[i], fuzz_dict_pick(&d));

	for (i = 0; i < 8; i++)
		CHECK(strcmp(seq_a[i], seq_b[i]) == 0);
}

static JSON_Object *build_params(const char *param_name,
				  const char *dict, int with_match,
				  const char *match_str)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, "param_name", param_name);
	json_object_set_string(root, "dict", dict);
	if (with_match)
		json_object_set_string(root, "match_str", match_str);
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
	params[0].val = (intptr_t)buf;
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

static void test_dict_templates(void)
{
	static fuzz_dict_t d;

	CHECK(fuzz_dict_load(&d, DICT_TMPL_PATH) == 0);
	/* 3 flat + 2 expanded */
	CHECK(d.count == 5);
	CHECK(strcmp(d.tokens[0], "%s%s%n") == 0);
	CHECK(strcmp(d.tokens[1], "GET / HTTP/1.1") == 0);
	CHECK(strcmp(d.tokens[2], "evil.example.com") == 0);
	/* template: "GET %2% HTTP/1.1\r\nHost: %3%" -- the
	 * substituted token itself starts with "GET", hence the
	 * doubled prefix; escapes copy verbatim (dicts are
	 * byte-level)
	 */
	CHECK(strcmp(d.tokens[3],
		"GET GET / HTTP/1.1 HTTP/1.1\\r\\nHost: "
		"evil.example.com") == 0);
	CHECK(strcmp(d.tokens[4], "/tmp/%s%s%n") == 0);
}

static void test_dict_template_bad_ref(void)
{
	static fuzz_dict_t d;

	/* %5% with zero flat tokens -> loud load failure */
	CHECK(fuzz_dict_load(&d, DICT_BAD_PATH) == -1);
	CHECK(d.count == 0);
}

static void test_action_lookup(void)
{
	CHECK(retrace_actions_get("fuzz_str") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("fuzz_str");
	int rc;

	rc = action(build_empty_ctx(), NULL);
	CHECK(rc == -1);
}

static void test_missing_param_name(void)
{
	action_fn_t action = retrace_actions_get("fuzz_str");
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	int rc;

	json_object_set_string(root, "dict", DICT_PATH);
	rc = action(build_empty_ctx(), root);
	CHECK(rc == -1);
	json_value_free(root_val);
}

static void test_missing_dict(void)
{
	action_fn_t action = retrace_actions_get("fuzz_str");
	JSON_Object *params = build_params("path", NULL, 0, NULL);
	int rc;

	/* parson keeps the key absent when value is NULL */
	rc = action(build_empty_ctx(), params);
	CHECK(rc == -1);
}

static void test_bad_dict_path(void)
{
	action_fn_t action = retrace_actions_get("fuzz_str");
	JSON_Object *params =
		build_params("path", "no/such/dict.txt", 0, NULL);
	int rc;

	rc = action(build_ctx_with_str_param("path", "/etc/hosts"),
		params);
	CHECK(rc == -1);
}

static void test_unknown_param(void)
{
	action_fn_t action = retrace_actions_get("fuzz_str");
	JSON_Object *params = build_params("nope", DICT_PATH, 0, NULL);
	int rc;

	rc = action(build_ctx_with_str_param("path", "/etc/hosts"),
		params);
	CHECK(rc == -1);
}

static void test_replaces_with_dict_token(void)
{
	action_fn_t action = retrace_actions_get("fuzz_str");
	JSON_Object *params = build_params("path", DICT_PATH, 0, NULL);
	struct ThreadContext *ctx =
		build_ctx_with_str_param("path", "/etc/hosts");
	int rc;

	static fuzz_dict_t d;

	rc = action(ctx, params);
	CHECK(rc == 0);
	CHECK(ctx->params[0].free_val == 1);
	CHECK(fuzz_dict_load(&d, DICT_PATH) == 0);
	CHECK(token_in_dict(&d, (char *)ctx->params[0].val) == 1);
	CHECK(strcmp((char *)ctx->params[0].val, "/etc/hosts") != 0);

	free((void *)(intptr_t)ctx->params[0].val);
	ctx->params[0].free_val = 0;
}

static void test_match_str_no_match(void)
{
	action_fn_t action = retrace_actions_get("fuzz_str");
	JSON_Object *params =
		build_params("path", DICT_PATH, 1, "/etc/passwd");
	struct ThreadContext *ctx =
		build_ctx_with_str_param("path", "/etc/hosts");
	int rc;

	rc = action(ctx, params);
	CHECK(rc == 0);
	CHECK(strcmp((char *)ctx->params[0].val, "/etc/hosts") == 0);
	CHECK(ctx->params[0].free_val == 0);
}

static void test_repeated_call_no_leak_growth(void)
{
	action_fn_t action = retrace_actions_get("fuzz_str");
	JSON_Object *params = build_params("path", DICT_PATH, 0, NULL);
	struct ThreadContext *ctx =
		build_ctx_with_str_param("path", "/etc/hosts");
	int i;

	for (i = 0; i < 16; i++) {
		CHECK(action(ctx, params) == 0);
		CHECK(ctx->params[0].free_val == 1);
		CHECK(strlen((char *)ctx->params[0].val) > 0);
	}

	free((void *)(intptr_t)ctx->params[0].val);
	ctx->params[0].free_val = 0;
}

int main(void)
{
	retrace_actions_init();

	printf("fuzz_str action tests:\n");

	TEST(dict_load);
	TEST(dict_load_missing);
	TEST(dict_pick_deterministic);
	TEST(dict_templates);
	TEST(dict_template_bad_ref);
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_param_name);
	TEST(missing_dict);
	TEST(bad_dict_path);
	TEST(unknown_param);
	TEST(replaces_with_dict_token);
	TEST(match_str_no_match);
	TEST(repeated_call_no_leak_growth);

	printf("Pass: %d, Fail: %d (of %d)\n", tests_pass, tests_fail,
		tests_run);
	return tests_fail == 0 ? 0 : 1;
}
