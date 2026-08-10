/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Micro-benchmark: call_real action cost (TODO.complete/34 P0).
 *
 * call_real invokes the configured real_impl pointer via
 * retrace_as_call_real_dispatch + measures timing via
 * clock_gettime. The bench installs a stub function as
 * real_impl to isolate the action cost from libc variability.
 *
 * Variants:
 *   - zero-arg stub (params_cnt=0)
 *   - one-arg stub  (params_cnt=1, value 99)
 *
 * The timing JSON entry is logged via retrace_logger_log_json;
 * not captured here.
 */

#include "bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "actions.h"
#include "arch_spec.h"
#include "funcs.h"
#include "data_types.h"
#include "real_impls.h"

typedef int (*action_fn_t)(struct ThreadContext *t_ctx,
			    const JSON_Object *action_params);

static action_fn_t g_call_real;

static long g_zero_call_count;
static long g_one_call_last_arg;

static long stub_zero(void)
{
	g_zero_call_count++;
	return 0x4242;
}

static long stub_one(long a)
{
	g_one_call_last_arg = a;
	return a + 1;
}

struct bench_ctx {
	struct ThreadContext tctx;
	struct FuncPrototype proto;
	struct FuncParam params[8];
};

static void setup_zero_arg(struct bench_ctx *c)
{
	const struct DataType *int_dt = retrace_datatype_get("int");

	memset(c, 0, sizeof(*c));
	memset(&c->proto, 0, sizeof(c->proto));
	strncpy(c->proto.name, "stub", sizeof(c->proto.name) - 1);
	c->proto.fmt = FAT_NOVARARGS;
	c->tctx.prototype = &c->proto;
	c->tctx.real_impl = stub_zero;
	c->tctx.params_cnt = 0;
	(void)int_dt;
}

static void setup_one_arg(struct bench_ctx *c)
{
	const struct DataType *int_dt = retrace_datatype_get("int");

	memset(c, 0, sizeof(*c));
	memset(&c->proto, 0, sizeof(c->proto));
	memset(c->params, 0, sizeof(c->params));

	strncpy(c->proto.name, "stub1", sizeof(c->proto.name) - 1);
	c->proto.fmt = FAT_NOVARARGS;
	c->tctx.prototype = &c->proto;
	c->tctx.real_impl = stub_one;

	strncpy(c->params[0].param_meta.name, "p0",
		sizeof(c->params[0].param_meta.name) - 1);
	c->params[0].param_meta.modifiers = CDM_NOMOD;
	c->params[0].param_meta.direction = PDIR_IN;
	c->params[0].data_type = int_dt;
	c->params[0].val = 99;

	c->tctx.params_cnt = 1;
	memcpy(c->tctx.params, c->params, sizeof(c->params));
}

static void bench_op(void *ctx)
{
	struct bench_ctx *c = (struct bench_ctx *)ctx;

	(void)g_call_real(&c->tctx, NULL);
}

int main(void)
{
	struct bench_result r;
	struct bench_ctx cz;
	struct bench_ctx co;

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

	retrace_actions_init();
	retrace_datatypes_init();

	g_call_real = retrace_actions_get("call_real");
	if (g_call_real == NULL) {
		fprintf(stderr, "FATAL: call_real action not registered\n");
		return 1;
	}

	setup_zero_arg(&cz);
	setup_one_arg(&co);

	g_zero_call_count = 0;
	g_one_call_last_arg = -1;

	printf("--- call_real benchmark ---\n");

	if (bench_run("call_real_zero_arg", bench_op, &cz, 10000, &r) == 0)
		bench_print("call_real_zero_arg", &r);

	if (bench_run("call_real_one_arg", bench_op, &co, 10000, &r) == 0)
		bench_print("call_real_one_arg", &r);

	return 0;
}
