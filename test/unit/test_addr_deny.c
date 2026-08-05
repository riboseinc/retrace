/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the addr_deny action (TODO.complete/15).
 *
 * The action is registered by name in __retrace_acts; we look it
 * up via retrace_actions_get("addr_deny") and call it directly
 * with a constructed ThreadContext. This bypasses the engine and
 * the trampoline, exercising only the action's own logic.
 *
 * Test cases:
 *   - Action lookup succeeds
 *   - Required-params validation (NULL params, missing deny_addrs)
 *   - Match on IPv4 exact (DENY)
 *   - Match on IPv4 wildcard port (DENY)
 *   - Match on deny-all "*" (DENY)
 *   - No-match passes through
 *   - sockaddr extraction: finds param named "addr"
 *   - sockaddr extraction: finds param named "dest_addr" (sendto)
 *   - sockaddr extraction: finds param named "src_addr" (recvfrom)
 *   - sockaddr extraction: no addr param -> passthrough
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "engine.h"
#include "actions.h"
#include "funcs.h"
#include "data_types.h"
#include "real_impls.h"
#include "sockaddr_inspect.h"
#include "parson.h"

/* Function pointer type matching the Action.callback signature.
 * checkpatch requires parameter names even in function pointer
 * typedefs; centralizing here avoids repeating the long decl.
 */
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

/* Build a JSON object with a single key -> array of strings. */
static JSON_Object *build_params(const char *key, const char **vals, int n)
{
	JSON_Value *root_val;
	JSON_Object *root;
	JSON_Value *arr_val;
	JSON_Array *arr;
	int i;

	root_val = json_value_init_object();
	root = json_value_get_object(root_val);

	arr_val = json_value_init_array();
	arr = json_array(arr_val);
	for (i = 0; i < n; i++)
		json_array_append_string(arr, vals[i]);

	json_object_set_value(root, key, arr_val);
	return root;
}

/* Build a ThreadContext with a single param pointing to a sockaddr.
 * The param's name controls find_sockaddr()'s behavior.
 */
static struct ThreadContext *build_ctx_with_addr(const char *param_name,
						  const void *sa)
{
	static struct ThreadContext ctx;
	struct FuncParam params[8];

	memset(&ctx, 0, sizeof(ctx));
	memset(params, 0, sizeof(params));

	params[0].val = (long)sa;
	strncpy(params[0].param_meta.name, param_name,
		sizeof(params[0].param_meta.name) - 1);
	params[0].param_meta.name[sizeof(params[0].param_meta.name) - 1] = '\0';

	ctx.params_cnt = 1;
	memcpy(ctx.params, params, sizeof(params));
	ctx.ret_val = 0;

	return &ctx;
}

static struct ThreadContext *build_ctx_no_addr(void)
{
	static struct ThreadContext ctx;
	struct FuncParam params[8];

	memset(&ctx, 0, sizeof(ctx));
	memset(params, 0, sizeof(params));

	/* Param named "buf" -- find_sockaddr should NOT match. */
	params[0].val = 0;
	strcpy(params[0].param_meta.name, "buf");
	ctx.params_cnt = 1;
	memcpy(ctx.params, params, sizeof(params));
	ctx.ret_val = 0;

	return &ctx;
}

/* Build a sockaddr_in for the given ip:port. */
static void build_sockaddr_in(struct sockaddr_in *sin,
			      const char *ip, int port)
{
	memset(sin, 0, sizeof(*sin));
	sin->sin_family = AF_INET;
	sin->sin_port = htons((uint16_t)port);
	inet_pton(AF_INET, ip, &sin->sin_addr);
}

/* -- Tests -- */

static void test_action_lookup(void)
{
	retrace_actions_init();
	assert(retrace_actions_get("addr_deny") != NULL);
}

static void test_action_params_null(void)
{
	action_fn_t action;
	struct ThreadContext *ctx = build_ctx_no_addr();
	int rc;

	action = retrace_actions_get("addr_deny");
	assert(action != NULL);

	/* NULL params -> -1 with diagnostic. */
	rc = action(ctx, NULL);
	assert(rc == -1);
}

static void test_action_missing_deny_addrs(void)
{
	action_fn_t action;
	struct ThreadContext *ctx = build_ctx_no_addr();
	JSON_Value *root_val;
	JSON_Object *root;
	int rc;

	action = retrace_actions_get("addr_deny");
	assert(action != NULL);

	/* Empty JSON object, no deny_addrs key. */
	root_val = json_value_init_object();
	root = json_value_get_object(root_val);

	rc = action(ctx, root);
	assert(rc == -1);

	json_value_free(root_val);
}

static void test_match_ipv4_exact(void)
{
	action_fn_t action;
	struct sockaddr_in sin;
	struct ThreadContext *ctx;
	const char *specs[] = { "127.0.0.1:443" };
	JSON_Object *params;
	int rc;

	build_sockaddr_in(&sin, "127.0.0.1", 443);
	ctx = build_ctx_with_addr("addr", &sin);

	params = build_params("deny_addrs", specs, 1);

	action = retrace_actions_get("addr_deny");
	rc = action(ctx, params);

	/* Match -> action returns -1, sets ret_val to -ECONNREFUSED. */
	assert(rc == -1);
	assert(ctx->ret_val == -ECONNREFUSED);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_match_ipv4_wildcard_port(void)
{
	action_fn_t action;
	struct sockaddr_in sin;
	struct ThreadContext *ctx;
	const char *specs[] = { "127.0.0.1:*" };
	JSON_Object *params;
	int rc;

	build_sockaddr_in(&sin, "127.0.0.1", 443);
	ctx = build_ctx_with_addr("addr", &sin);

	params = build_params("deny_addrs", specs, 1);

	action = retrace_actions_get("addr_deny");
	rc = action(ctx, params);

	assert(rc == -1);
	assert(ctx->ret_val == -ECONNREFUSED);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_match_deny_all(void)
{
	action_fn_t action;
	struct sockaddr_in sin;
	struct ThreadContext *ctx;
	const char *specs[] = { "*" };
	JSON_Object *params;
	int rc;

	build_sockaddr_in(&sin, "10.0.0.5", 8080);
	ctx = build_ctx_with_addr("addr", &sin);

	params = build_params("deny_addrs", specs, 1);

	action = retrace_actions_get("addr_deny");
	rc = action(ctx, params);

	assert(rc == -1);
	assert(ctx->ret_val == -ECONNREFUSED);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_no_match_passes_through(void)
{
	action_fn_t action;
	struct sockaddr_in sin;
	struct ThreadContext *ctx;
	const char *specs[] = { "10.0.0.99:*" };
	JSON_Object *params;
	int rc;

	build_sockaddr_in(&sin, "10.0.0.1", 443);
	ctx = build_ctx_with_addr("addr", &sin);

	params = build_params("deny_addrs", specs, 1);

	action = retrace_actions_get("addr_deny");
	rc = action(ctx, params);

	/* No match -> action returns 0, ret_val unchanged. */
	assert(rc == 0);
	assert(ctx->ret_val == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_find_addr_param_name_variants(void)
{
	action_fn_t action;
	struct sockaddr_in sin;
	const char *specs[] = { "*" };
	JSON_Object *params;
	const char *names[] = { "addr", "dest_addr", "src_addr" };
	int i;

	action = retrace_actions_get("addr_deny");
	build_sockaddr_in(&sin, "1.2.3.4", 80);
	params = build_params("deny_addrs", specs, 1);

	for (i = 0; i < 3; i++) {
		struct ThreadContext *ctx = build_ctx_with_addr(names[i], &sin);
		int rc = action(ctx, params);

		assert(rc == -1);
		assert(ctx->ret_val == -ECONNREFUSED);
	}

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_no_addr_param_passes_through(void)
{
	action_fn_t action;
	struct ThreadContext *ctx;
	const char *specs[] = { "*" };
	JSON_Object *params;
	int rc;

	ctx = build_ctx_no_addr();
	params = build_params("deny_addrs", specs, 1);

	action = retrace_actions_get("addr_deny");
	rc = action(ctx, params);

	/* No sockaddr param -> action returns 0 (passthrough). */
	assert(rc == 0);
	assert(ctx->ret_val == 0);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_first_match_wins(void)
{
	action_fn_t action;
	struct sockaddr_in sin;
	struct ThreadContext *ctx;
	const char *specs[] = { "10.0.0.99:*", "*:443", "1.2.3.4:80" };
	JSON_Object *params;
	int rc;

	build_sockaddr_in(&sin, "10.0.0.1", 443);
	ctx = build_ctx_with_addr("addr", &sin);

	params = build_params("deny_addrs", specs, 3);

	action = retrace_actions_get("addr_deny");
	rc = action(ctx, params);

	/* Second spec (*:443) matches. */
	assert(rc == -1);
	assert(ctx->ret_val == -ECONNREFUSED);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_malformed_spec_ignored(void)
{
	action_fn_t action;
	struct sockaddr_in sin;
	struct ThreadContext *ctx;
	const char *specs[] = { "[unclosed", "1.2.3.4:443" };
	JSON_Object *params;
	int rc;

	build_sockaddr_in(&sin, "1.2.3.4", 443);
	ctx = build_ctx_with_addr("addr", &sin);

	params = build_params("deny_addrs", specs, 2);

	action = retrace_actions_get("addr_deny");
	rc = action(ctx, params);

	/* First spec malformed (warned + skipped), second matches. */
	assert(rc == -1);
	assert(ctx->ret_val == -ECONNREFUSED);

	json_value_free(json_object_get_wrapping_value(params));
}

int main(void)
{
	/* Set up minimal retrace_real_impls for the helper + actions. */
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;
	retrace_real_impls.real_vsnprintf = vsnprintf;

	printf("addr_deny action tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(action_params_null);
	TEST(action_missing_deny_addrs);

	printf("  -- match paths --\n");
	TEST(match_ipv4_exact);
	TEST(match_ipv4_wildcard_port);
	TEST(match_deny_all);

	printf("  -- non-match paths --\n");
	TEST(no_match_passes_through);
	TEST(no_addr_param_passes_through);

	printf("  -- sockaddr extraction --\n");
	TEST(find_addr_param_name_variants);

	printf("  -- array semantics --\n");
	TEST(first_match_wins);
	TEST(malformed_spec_ignored);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
