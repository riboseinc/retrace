/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Micro-benchmark: log_params action cost (TODO.complete/34 P0).
 *
 * log_params is the most-used action -- the default config runs
 * it for every intercepted call. This benchmark measures the
 * per-call cost of serializing N int params to JSON.
 *
 * Variants:
 *   - 1 param   (common: open(path, flags))
 *   - 4 params  (medium: socket/fork with full frame)
 *   - 8 params  (large: rare but possible)
 *
 * Setup mirrors test_log_params.c: per-param DataType is
 * retrace_datatype_get("int") after retrace_datatypes_init().
 */

#include "bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "actions.h"
#include "data_types.h"
#include "real_impls.h"

typedef int (*action_fn_t)(struct ThreadContext *t_ctx,
			    const JSON_Object *action_params);

static action_fn_t g_log_params;

struct bench_ctx {
	struct ThreadContext tctx;
	struct FuncPrototype proto;
	struct FuncParam params[8];
	int n_params;
};

static void setup_ctx(struct bench_ctx *c, int n_params)
{
	const struct DataType *int_dt;
	int i;

	int_dt = retrace_datatype_get("int");
	if (int_dt == NULL) {
		fprintf(stderr, "FATAL: datatype 'int' not registered\n");
		exit(1);
	}

	memset(c, 0, sizeof(*c));
	memset(&c->proto, 0, sizeof(c->proto));

	strncpy(c->proto.name, "bench_func", sizeof(c->proto.name) - 1);
	c->proto.fmt = FAT_NOVARARGS;
	c->tctx.prototype = &c->proto;

	for (i = 0; i < n_params && i < 8; i++) {
		char nm[8];

		snprintf(nm, sizeof(nm), "p%d", i);
		strncpy(c->params[i].param_meta.name, nm,
			sizeof(c->params[i].param_meta.name) - 1);
		strcpy(c->params[i].param_meta.type_name, "int");
		c->params[i].param_meta.modifiers = CDM_NOMOD;
		c->params[i].param_meta.direction = PDIR_IN;
		c->params[i].data_type = int_dt;
		c->params[i].val = i + 1;
	}

	c->tctx.params_cnt = n_params;
	memcpy(c->tctx.params, c->params, sizeof(c->params));
	c->n_params = n_params;
}

static void bench_op(void *ctx)
{
	struct bench_ctx *c = (struct bench_ctx *)ctx;

	(void)g_log_params(&c->tctx, NULL);
}

int main(void)
{
	struct bench_result r;
	struct bench_ctx c1;
	struct bench_ctx c4;
	struct bench_ctx c8;

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

	g_log_params = retrace_actions_get("log_params");
	if (g_log_params == NULL) {
		fprintf(stderr, "FATAL: log_params action not registered\n");
		return 1;
	}

	setup_ctx(&c1, 1);
	setup_ctx(&c4, 4);
	setup_ctx(&c8, 8);

	printf("--- log_params benchmark ---\n");

	if (bench_run("log_params_1_int", bench_op, &c1, 10000, &r) == 0)
		bench_print("log_params_1_int", &r);

	if (bench_run("log_params_4_ints", bench_op, &c4, 10000, &r) == 0)
		bench_print("log_params_4_ints", &r);

	if (bench_run("log_params_8_ints", bench_op, &c8, 10000, &r) == 0)
		bench_print("log_params_8_ints", &r);

	return 0;
}
