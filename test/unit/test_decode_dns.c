/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the decode_dns action (TODO.complete/23).
 *
 * Tests verify DNS query parsing (A, AAAA records), response
 * parsing, non-DNS passthrough, and edge cases.
 */

#include "test_utils.h"

DECLARE_TEST_STATE();

static struct ThreadContext *build_ctx_with_buf(const char *name,
						const unsigned char *data,
						int data_len)
{
	static struct ThreadContext ctx;
	static struct FuncPrototype proto;
	static unsigned char buf[4096];
	struct FuncParam params[8];

	memset(&ctx, 0, sizeof(ctx));
	memset(params, 0, sizeof(params));
	memset(&proto, 0, sizeof(proto));

	strncpy(proto.name, "sendto", sizeof(proto.name) - 1);
	ctx.prototype = &proto;

	if (data && data_len > 0) {
		memcpy(buf, data, data_len);
	} else {
		memset(buf, 0, sizeof(buf));
	}

	strncpy(params[0].param_meta.name, name,
		sizeof(params[0].param_meta.name) - 1);
	params[0].param_meta.modifiers = CDM_POINTER;
	params[0].param_meta.direction = PDIR_IN;
	params[0].val = (long)buf;

	strncpy(params[1].param_meta.name, "len",
		sizeof(params[1].param_meta.name) - 1);
	params[1].param_meta.direction = PDIR_IN;
	params[1].val = data_len;

	ctx.params_cnt = 2;
	memcpy(ctx.params, params, sizeof(params));
	return &ctx;
}

static JSON_Object *build_params(const char *param_name)
{
	return build_json_string_kv("param_name", param_name);
}

/* Build a DNS query for example.com type A (1). */
static void build_dns_query(unsigned char *buf, int *out_len)
{
	/* Header: ID=0x1234, flags=0x0100 (RD), QDCOUNT=1 */
	unsigned char header[] = {
		0x12, 0x34, /* ID */
		0x01, 0x00, /* flags: RD */
		0x00, 0x01, /* QDCOUNT */
		0x00, 0x00, /* ANCOUNT */
		0x00, 0x00, /* NSCOUNT */
		0x00, 0x00  /* ARCOUNT */
	};
	/* Question: example.com type A class IN */
	unsigned char question[] = {
		7, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
		3, 'c', 'o', 'm',
		0, /* null label */
		0x00, 0x01, /* QTYPE = A */
		0x00, 0x01  /* QCLASS = IN */
	};

	memcpy(buf, header, sizeof(header));
	memcpy(buf + sizeof(header), question, sizeof(question));
	*out_len = sizeof(header) + sizeof(question);
}

static void build_dns_aaaa_query(unsigned char *buf, int *out_len)
{
	unsigned char header[] = {
		0xAB, 0xCD, 0x01, 0x00,
		0x00, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	};
	unsigned char question[] = {
		3, 'f', 'o', 'o',
		3, 'b', 'a', 'r',
		0,
		0x00, 0x1C, /* QTYPE = AAAA */
		0x00, 0x01
	};

	memcpy(buf, header, sizeof(header));
	memcpy(buf + sizeof(header), question, sizeof(question));
	*out_len = sizeof(header) + sizeof(question);
}

static void build_dns_response(unsigned char *buf, int *out_len)
{
	unsigned char header[] = {
		0x12, 0x34,
		0x81, 0x80, /* flags: QR + RD + RA */
		0x00, 0x01, /* QDCOUNT */
		0x00, 0x01, /* ANCOUNT */
		0x00, 0x00, 0x00, 0x00
	};
	unsigned char question[] = {
		7, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
		3, 'c', 'o', 'm',
		0,
		0x00, 0x01, 0x00, 0x01
	};

	memcpy(buf, header, sizeof(header));
	memcpy(buf + sizeof(header), question, sizeof(question));
	*out_len = sizeof(header) + sizeof(question);
}

static void test_action_lookup(void)
{
	retrace_actions_init();
	CHECK(retrace_actions_get("decode_dns") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("decode_dns");
	struct ThreadContext ctx;
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	rc = action(&ctx, NULL);
	CHECK(rc == -1);
}

static void test_missing_param_name(void)
{
	action_fn_t action = retrace_actions_get("decode_dns");
	struct ThreadContext ctx;
	JSON_Value *v = json_value_init_object();
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	rc = action(&ctx, json_value_get_object(v));
	CHECK(rc == -1);
	json_value_free(v);
}

static void test_dns_a_query(void)
{
	action_fn_t action = retrace_actions_get("decode_dns");
	unsigned char dns_buf[256];
	int dns_len;
	struct ThreadContext *ctx;
	JSON_Object *p;
	int rc;

	build_dns_query(dns_buf, &dns_len);
	ctx = build_ctx_with_buf("buf", dns_buf, dns_len);
	p = build_params("buf");

	rc = action(ctx, p);
	CHECK(rc == 0);

	json_value_free(json_object_get_wrapping_value(p));
}

static void test_dns_aaaa_query(void)
{
	action_fn_t action = retrace_actions_get("decode_dns");
	unsigned char dns_buf[256];
	int dns_len;
	struct ThreadContext *ctx;
	JSON_Object *p;
	int rc;

	build_dns_aaaa_query(dns_buf, &dns_len);
	ctx = build_ctx_with_buf("buf", dns_buf, dns_len);
	p = build_params("buf");

	rc = action(ctx, p);
	CHECK(rc == 0);

	json_value_free(json_object_get_wrapping_value(p));
}

static void test_dns_response(void)
{
	action_fn_t action = retrace_actions_get("decode_dns");
	unsigned char dns_buf[256];
	int dns_len;
	struct ThreadContext *ctx;
	JSON_Object *p;
	int rc;

	build_dns_response(dns_buf, &dns_len);
	ctx = build_ctx_with_buf("buf", dns_buf, dns_len);
	p = build_params("buf");

	rc = action(ctx, p);
	CHECK(rc == 0);

	json_value_free(json_object_get_wrapping_value(p));
}

static void test_non_dns_data(void)
{
	action_fn_t action = retrace_actions_get("decode_dns");
	unsigned char raw[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	struct ThreadContext *ctx = build_ctx_with_buf("buf", raw, sizeof(raw));
	JSON_Object *p = build_params("buf");
	int rc;

	rc = action(ctx, p);
	CHECK(rc == 0);

	json_value_free(json_object_get_wrapping_value(p));
}

static void test_short_buffer(void)
{
	action_fn_t action = retrace_actions_get("decode_dns");
	unsigned char short_buf[] = {0x01, 0x02, 0x03};
	struct ThreadContext *ctx = build_ctx_with_buf("buf", short_buf, 3);
	JSON_Object *p = build_params("buf");
	int rc;

	rc = action(ctx, p);
	CHECK(rc == 0);

	json_value_free(json_object_get_wrapping_value(p));
}

static void test_null_buffer(void)
{
	action_fn_t action = retrace_actions_get("decode_dns");
	static struct ThreadContext ctx;
	static struct FuncPrototype proto;
	struct FuncParam params[8];
	JSON_Object *p = build_params("buf");
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	memset(params, 0, sizeof(params));
	memset(&proto, 0, sizeof(proto));
	strncpy(proto.name, "sendto", sizeof(proto.name) - 1);
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
	action_fn_t action = retrace_actions_get("decode_dns");
	unsigned char dns_buf[256];
	int dns_len;
	struct ThreadContext *ctx;
	JSON_Object *p;
	int rc;

	build_dns_query(dns_buf, &dns_len);
	ctx = build_ctx_with_buf("buf", dns_buf, dns_len);
	p = build_params("nonexistent");

	rc = action(ctx, p);
	CHECK(rc == 0);
	json_value_free(json_object_get_wrapping_value(p));
}

int main(void)
{
	init_minimal_real_impls();

	printf("decode_dns action tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_param_name);
	TEST(unknown_param);

	printf("  -- DNS queries --\n");
	TEST(dns_a_query);
	TEST(dns_aaaa_query);

	printf("  -- DNS response --\n");
	TEST(dns_response);

	printf("  -- non-DNS + edge cases --\n");
	TEST(non_dns_data);
	TEST(short_buffer);
	TEST(null_buffer);

	return finish_tests(tests_run, tests_pass, tests_fail);
}
