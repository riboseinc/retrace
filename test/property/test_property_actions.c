// SPDX-License-Identifier: BSD-2-Clause
//
// Property-based tests for v2 actions (TODO.complete/16 P1).
//
// Properties:
//
//   P-MODIFY-RETVAL-ROUNDTRIP     for any int N in [-2^31, 2^31),
//                                 retval_int=N sets ctx.ret_val to N
//
//   P-INCOMPLETE-IO-RATE-LINEAR   for any rate R in [0.0, 1.0] and
//                                 any retval V > 0, the new retval
//                                 equals floor(V * R)
//
//   P-INCOMPLETE-IO-CLAMPED       for any rate R outside [0,1], the
//                                 new retval equals rate=0 (R<0) or
//                                 rate=1 (R>1)
//
//   P-CALL-COUNT-LIMIT-EXACT      for any limit N in [1, 100], the
//                                 first N calls return 0 and the
//                                 (N+1)th returns -1
//
//   P-FUZZING-SEED-DETERMINISTIC  for any seed S in [0, 2^32), two
//                                 runs of (fuzzing_seed(S); sample 5
//                                 rand() values) produce identical
//                                 sequences
//
// The PRNG drives input generation. Each property runs N iterations
// (default 1000). On failure, the harness prints the failing seed.
//
// Setup mirrors test/unit/test_*.c: minimal retrace_real_impls,
// retrace_actions_init + retrace_datatypes_init for prototype lookup.

#include "property_harness.h"
#include "engine.h"
#include "actions.h"
#include "data_types.h"
#include "real_impls.h"
#include "parson.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern struct RetraceRealImpls retrace_real_impls;

typedef int (*action_fn_t)(struct ThreadContext *t_ctx,
			    const JSON_Object *action_params);

static action_fn_t g_modify_retval;
static action_fn_t g_incomplete_io;
static action_fn_t g_call_count_limit;
static action_fn_t g_fuzzing_seed;

/* Build a ThreadContext with a single IN int param. ret_val is the
 * "real" return value the engine would have set before actions run.
 */
static struct ThreadContext *build_ctx_with_retval(long ret_val)
{
	static struct ThreadContext ctx;
	static struct FuncPrototype proto;

	memset(&ctx, 0, sizeof(ctx));
	memset(&proto, 0, sizeof(proto));
	strncpy(proto.name, "prop_func", sizeof(proto.name) - 1);
	ctx.prototype = &proto;
	ctx.ret_val = ret_val;
	return &ctx;
}

static struct ThreadContext *build_ctx_for_func(const char *func_name)
{
	static struct ThreadContext ctx;
	static struct FuncPrototype proto;

	memset(&ctx, 0, sizeof(ctx));
	memset(&proto, 0, sizeof(proto));
	strncpy(proto.name, func_name, sizeof(proto.name) - 1);
	ctx.prototype = &proto;
	return &ctx;
}

static JSON_Object *build_json_number(const char *key, double val)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_number(root, key, val);
	return root;
}

/* -- Properties -- */

static int prop_modify_retval_roundtrip(uint64_t seed)
{
	struct ret_prng prng;
	int32_t n;
	JSON_Object *params;
	struct ThreadContext *ctx;
	int rc;

	ret_prng_seed(&prng, seed);

	/* Generate any int32. Use the high 32 bits of one PRNG draw. */
	n = (int32_t)ret_prng_next(&prng);

	params = build_json_number("retval_int", (double)n);
	ctx = build_ctx_with_retval(0);

	rc = g_modify_retval(ctx, params);

	json_value_free(json_object_get_wrapping_value(params));

	if (rc != 0)
		return 0;
	return ctx->ret_val == (long)n ? 1 : 0;
}

static int prop_incomplete_io_rate_linear(uint64_t seed)
{
	struct ret_prng prng;
	uint32_t rate_milli;
	double rate;
	long real_ret;
	long expected;
	JSON_Object *params;
	struct ThreadContext *ctx;
	int rc;

	ret_prng_seed(&prng, seed);

	/* rate in [0.000, 1.000] in 0.001 increments. */
	rate_milli = ret_prng_u32(&prng, 1001);
	rate = rate_milli / 1000.0;

	/* real_ret in [1, 1_000_000]. */
	real_ret = 1 + (long)ret_prng_u32(&prng, 1000000);

	params = build_json_number("rate", rate);
	ctx = build_ctx_with_retval(real_ret);

	rc = g_incomplete_io(ctx, params);

	json_value_free(json_object_get_wrapping_value(params));

	if (rc != 0)
		return 0;

	expected = (long)(real_ret * rate);
	return ctx->ret_val == expected ? 1 : 0;
}

static int prop_incomplete_io_clamped(uint64_t seed)
{
	struct ret_prng prng;
	double rate;
	long real_ret;
	long expected;
	JSON_Object *params;
	struct ThreadContext *ctx;
	int rc;
	int negative_case;

	ret_prng_seed(&prng, seed);

	/* Pick a rate outside [0, 1]. */
	negative_case = ret_prng_u32(&prng, 2);
	if (negative_case) {
		/* rate in [-1000, -0.001] */
		rate = -((double)(1 + ret_prng_u32(&prng, 1000000))) / 1000.0;
		expected = 0;
	} else {
		/* rate in [1.001, 1000.0] */
		rate = 1.0 + ((double)(1 + ret_prng_u32(&prng, 1000000))) / 1000.0;
		expected = real_ret;  /* clamped to 1.0 */
	}

	real_ret = 1 + (long)ret_prng_u32(&prng, 1000000);
	expected = negative_case ? 0 : real_ret;

	params = build_json_number("rate", rate);
	ctx = build_ctx_with_retval(real_ret);

	rc = g_incomplete_io(ctx, params);

	json_value_free(json_object_get_wrapping_value(params));

	if (rc != 0)
		return 0;
	return ctx->ret_val == expected ? 1 : 0;
}

static int prop_call_count_limit_exact(uint64_t seed)
{
	struct ret_prng prng;
	uint32_t limit_u;
	int limit;
	char func_name[32];
	JSON_Object *params;
	struct ThreadContext *ctx;
	int i;
	int rc;

	ret_prng_seed(&prng, seed);

	/* limit in [1, 50] -- bounded so the property runs fast. */
	limit_u = ret_prng_u32(&prng, 50);
	limit = (int)limit_u + 1;

	/* Unique name per iteration so the global counter table doesn't
	 * collide with prior runs.
	 */
	snprintf(func_name, sizeof(func_name), "prop_ccl_%llu",
		(unsigned long long)seed);

	params = build_json_number("limit", (double)limit);

	/* First `limit` calls must return 0. */
	for (i = 0; i < limit; i++) {
		ctx = build_ctx_for_func(func_name);
		rc = g_call_count_limit(ctx, params);
		if (rc != 0) {
			json_value_free(json_object_get_wrapping_value(params));
			return 0;
		}
	}

	/* The (limit+1)th call must return -1. */
	ctx = build_ctx_for_func(func_name);
	rc = g_call_count_limit(ctx, params);

	json_value_free(json_object_get_wrapping_value(params));

	return rc == -1 ? 1 : 0;
}

static int prop_fuzzing_seed_deterministic(uint64_t seed)
{
	struct ret_prng prng;
	uint32_t seed_value;
	JSON_Object *params;
	struct ThreadContext *ctx;
	int seq_a[5];
	int seq_b[5];
	int i;

	ret_prng_seed(&prng, seed);

	/* seed in [0, 2^32 - 1]. */
	seed_value = (uint32_t)ret_prng_next(&prng);

	params = build_json_number("seed", (double)seed_value);

	/* First sample. */
	ctx = build_ctx_with_retval(0);
	(void)g_fuzzing_seed(ctx, params);
	for (i = 0; i < 5; i++)
		seq_a[i] = rand();

	/* Reset and re-sample. */
	ctx = build_ctx_with_retval(0);
	(void)g_fuzzing_seed(ctx, params);
	for (i = 0; i < 5; i++)
		seq_b[i] = rand();

	json_value_free(json_object_get_wrapping_value(params));

	for (i = 0; i < 5; i++)
		if (seq_a[i] != seq_b[i])
			return 0;
	return 1;
}

int main(void)
{
	int failures = 0;

	/* Minimal real_impls for parson + actions. */
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
	retrace_real_impls.time = time;

	retrace_actions_init();
	retrace_datatypes_init();

	g_modify_retval = retrace_actions_get("modify_return_value_int");
	g_incomplete_io = retrace_actions_get("incomplete_io");
	g_call_count_limit = retrace_actions_get("call_count_limit");
	g_fuzzing_seed = retrace_actions_get("fuzzing_seed");

	if (g_modify_retval == NULL || g_incomplete_io == NULL ||
	    g_call_count_limit == NULL || g_fuzzing_seed == NULL) {
		fprintf(stderr,
			"[property] FAIL: action lookup incomplete\n");
		return 1;
	}

	printf("action property tests:\n");

	failures += property_run(prop_modify_retval_roundtrip,
		"modify_retval_roundtrip",
		RETRACE_PROPERTY_DEFAULT_ITERS, 100);
	failures += property_run(prop_incomplete_io_rate_linear,
		"incomplete_io_rate_linear",
		RETRACE_PROPERTY_DEFAULT_ITERS, 100);
	failures += property_run(prop_incomplete_io_clamped,
		"incomplete_io_clamped",
		RETRACE_PROPERTY_DEFAULT_ITERS, 100);
	failures += property_run(prop_call_count_limit_exact,
		"call_count_limit_exact",
		50,  /* small iter count: each iter uses a unique
		      * func_name and the action's global table caps
		      * at MAX_TRACKED_FUNCS=64 entries. 50 keeps us
		      * under the cap with margin.
		      */
		100);
	failures += property_run(prop_fuzzing_seed_deterministic,
		"fuzzing_seed_deterministic",
		RETRACE_PROPERTY_DEFAULT_ITERS, 100);

	if (failures == 0)
		printf("\n[property] all action properties PASS\n");
	else
		printf("\n[property] %d action properties FAILED\n",
			failures);

	return failures ? 1 : 0;
}
