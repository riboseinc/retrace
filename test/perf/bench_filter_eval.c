/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Micro-benchmark: filter action evaluation cost
 * (TODO.complete/34 P0).
 *
 * The filter action (TODO 20) evaluates one param comparison
 * per call. This benchmark measures that cost at 1M iters
 * for match and mismatch paths.
 *
 * Setup: single int param "x" with val=42. Filter checks
 * "x == 42" (match) and "x == 99" (mismatch).
 */

#include "bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "actions.h"
#include "data_types.h"
#include "real_impls.h"
#include "parson.h"

typedef int (*action_fn_t)(struct ThreadContext *t_ctx,
			    const JSON_Object *action_params);

static action_fn_t g_filter;

struct bench_ctx {
	struct ThreadContext tctx;
	struct FuncPrototype proto;
	struct FuncParam params[8];
	JSON_Object *match_params;
	JSON_Object *mismatch_params;
};

static void setup(struct bench_ctx *c)
{
	const struct DataType *int_dt = retrace_datatype_get("int");

	memset(c, 0, sizeof(*c));
	memset(&c->proto, 0, sizeof(c->proto));

	strncpy(c->proto.name, "bench", sizeof(c->proto.name) - 1);
	c->tctx.prototype = &c->proto;

	strncpy(c->params[0].param_meta.name, "x",
		sizeof(c->params[0].param_meta.name) - 1);
	c->params[0].param_meta.direction = PDIR_IN;
	c->params[0].data_type = int_dt;
	c->params[0].val = 42;

	c->tctx.params_cnt = 1;
	memcpy(c->tctx.params, c->params, sizeof(c->params));

	{
		JSON_Value *v1 = json_value_init_object();
		JSON_Value *v2 = json_value_init_object();
		JSON_Object *o1 = json_value_get_object(v1);
		JSON_Object *o2 = json_value_get_object(v2);

		json_object_set_string(o1, "param_name", "x");
		json_object_set_string(o1, "op", "==");
		json_object_set_number(o1, "value", 42);
		c->match_params = o1;

		json_object_set_string(o2, "param_name", "x");
		json_object_set_string(o2, "op", "==");
		json_object_set_number(o2, "value", 99);
		c->mismatch_params = o2;
	}
}

static void bench_match(void *ctx)
{
	struct bench_ctx *c = (struct bench_ctx *)ctx;

	(void)g_filter(&c->tctx, c->match_params);
}

static void bench_mismatch(void *ctx)
{
	struct bench_ctx *c = (struct bench_ctx *)ctx;

	(void)g_filter(&c->tctx, c->mismatch_params);
}

int main(void)
{
	struct bench_result r;
	struct bench_ctx c;

	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;

	retrace_actions_init();
	retrace_datatypes_init();

	g_filter = retrace_actions_get("filter");
	if (g_filter == NULL) {
		fprintf(stderr, "FATAL: filter action not registered\n");
		return 1;
	}

	setup(&c);

	printf("--- filter_eval benchmark ---\n");

	if (bench_run("filter_match", bench_match, &c, 1000000, &r) == 0)
		bench_print("filter_match", &r);

	if (bench_run("filter_mismatch", bench_mismatch, &c, 1000000, &r) == 0)
		bench_print("filter_mismatch", &r);

	return 0;
}
