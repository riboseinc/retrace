/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the decode_http action (TODO.complete/23 MVP).
 *
 * The action reads a named buffer param, checks whether the
 * first line looks like HTTP/1.x, and logs the parsed fields.
 * Tests verify request parsing, response parsing, and
 * non-HTTP passthrough.
 */

#include "test_utils.h"

static struct ThreadContext *build_ctx_with_buf(const char *name,
						 const char *data)
{
	static struct ThreadContext ctx;
	static struct FuncPrototype proto;
	static char buf[8192];
	struct FuncParam params[8];

	memset(&ctx, 0, sizeof(ctx));
	memset(params, 0, sizeof(params));
	memset(&proto, 0, sizeof(proto));

	strncpy(proto.name, "send", sizeof(proto.name) - 1);
	ctx.prototype = &proto;

	if (data) {
		strncpy(buf, data, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
	} else {
		buf[0] = '\0';
	}

	strncpy(params[0].param_meta.name, name,
		sizeof(params[0].param_meta.name) - 1);
	params[0].param_meta.modifiers = CDM_POINTER;
	params[0].param_meta.direction = PDIR_IN;
	params[0].val = (long)buf;

	ctx.params_cnt = 1;
	memcpy(ctx.params, params, sizeof(params));
	return &ctx;
}

static JSON_Object *build_params(const char *param_name)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, "param_name", param_name);
	return root;
}

DECLARE_TEST_STATE();

static void test_action_lookup(void)
{
	retrace_actions_init();
	CHECK(retrace_actions_get("decode_http") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("decode_http");
	struct ThreadContext ctx;
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	rc = action(&ctx, NULL);
	CHECK(rc == -1);
}

static void test_missing_param_name(void)
{
	action_fn_t action = retrace_actions_get("decode_http");
	struct ThreadContext ctx;
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	rc = action(&ctx, root);
	CHECK(rc == -1);
	json_value_free(root_val);
}

static void test_get_request(void)
{
	action_fn_t action = retrace_actions_get("decode_http");
	struct ThreadContext *ctx = build_ctx_with_buf("buf",
		"GET /api/v1/users HTTP/1.1\r\nHost: example.com\r\n\r\n");
	JSON_Object *p = build_params("buf");
	int rc;

	rc = action(ctx, p);
	CHECK(rc == 0);

	json_value_free(json_object_get_wrapping_value(p));
}

static void test_post_request(void)
{
	action_fn_t action = retrace_actions_get("decode_http");
	struct ThreadContext *ctx = build_ctx_with_buf("buf",
		"POST /submit HTTP/1.1\r\nContent-Length: 10\r\n\r\nname=value");
	JSON_Object *p = build_params("buf");
	int rc;

	rc = action(ctx, p);
	CHECK(rc == 0);
	json_value_free(json_object_get_wrapping_value(p));
}

static void test_http_response(void)
{
	action_fn_t action = retrace_actions_get("decode_http");
	struct ThreadContext *ctx = build_ctx_with_buf("buf",
		"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html>");
	JSON_Object *p = build_params("buf");
	int rc;

	rc = action(ctx, p);
	CHECK(rc == 0);
	json_value_free(json_object_get_wrapping_value(p));
}

static void test_http_404_response(void)
{
	action_fn_t action = retrace_actions_get("decode_http");
	struct ThreadContext *ctx = build_ctx_with_buf("buf",
		"HTTP/1.1 404 Not Found\r\n\r\n");
	JSON_Object *p = build_params("buf");
	int rc;

	rc = action(ctx, p);
	CHECK(rc == 0);
	json_value_free(json_object_get_wrapping_value(p));
}

static void test_non_http_data(void)
{
	action_fn_t action = retrace_actions_get("decode_http");
	struct ThreadContext *ctx = build_ctx_with_buf("buf",
		"\x00\x01\x02\x03 not http at all");
	JSON_Object *p = build_params("buf");
	int rc;

	rc = action(ctx, p);
	CHECK(rc == 0);
	json_value_free(json_object_get_wrapping_value(p));
}

static void test_empty_buffer(void)
{
	action_fn_t action = retrace_actions_get("decode_http");
	struct ThreadContext *ctx = build_ctx_with_buf("buf", "");
	JSON_Object *p = build_params("buf");
	int rc;

	rc = action(ctx, p);
	CHECK(rc == 0);
	json_value_free(json_object_get_wrapping_value(p));
}

static void test_null_buffer(void)
{
	action_fn_t action = retrace_actions_get("decode_http");
	static struct ThreadContext ctx;
	static struct FuncPrototype proto;
	struct FuncParam params[8];
	JSON_Object *p = build_params("buf");
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	memset(params, 0, sizeof(params));
	memset(&proto, 0, sizeof(proto));
	strncpy(proto.name, "send", sizeof(proto.name) - 1);
	ctx.prototype = &proto;
	strncpy(params[0].param_meta.name, "buf",
		sizeof(params[0].param_meta.name) - 1);
	params[0].val = 0;
	ctx.params_cnt = 1;
	memcpy(ctx.params, params, sizeof(params));

	rc = action(&ctx, p);
	CHECK(rc == 0);
	json_value_free(json_object_get_wrapping_value(p));
}

static void test_unknown_param(void)
{
	action_fn_t action = retrace_actions_get("decode_http");
	struct ThreadContext *ctx = build_ctx_with_buf("buf",
		"GET / HTTP/1.1\r\n\r\n");
	JSON_Object *p = build_params("nonexistent");
	int rc;

	rc = action(ctx, p);
	CHECK(rc == 0);
	json_value_free(json_object_get_wrapping_value(p));
}

static void test_delete_request(void)
{
	action_fn_t action = retrace_actions_get("decode_http");
	struct ThreadContext *ctx = build_ctx_with_buf("buf",
		"DELETE /api/items/42 HTTP/1.1\r\n\r\n");
	JSON_Object *p = build_params("buf");
	int rc;

	rc = action(ctx, p);
	CHECK(rc == 0);
	json_value_free(json_object_get_wrapping_value(p));
}

int main(void)
{
	init_minimal_real_impls();
	INIT_TESTS();

	printf("decode_http action tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_param_name);
	TEST(unknown_param);

	printf("  -- HTTP requests --\n");
	TEST(get_request);
	TEST(post_request);
	TEST(delete_request);

	printf("  -- HTTP responses --\n");
	TEST(http_response);
	TEST(http_404_response);

	printf("  -- non-HTTP + edge cases --\n");
	TEST(non_http_data);
	TEST(empty_buffer);
	TEST(null_buffer);

	return finish_tests(tests_run, tests_pass, tests_fail);
}
