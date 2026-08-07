/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Micro-benchmark: script_resolver cost per call
 * (TODO.complete/34 P0).
 *
 * Measures the time to find an intercept_script entry in an
 * array. Uses a small constructed JSON array (3 entries: one
 * exact name match, one wildcard, one with caller_matches).
 *
 * Runs under ctest with label "perf". Not in the default suite
 * because timings vary by host; meant for manual runs and a
 * future perf.yml workflow that compares against a baseline.
 */

#include "bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parson.h"

#include "real_impls.h"

static JSON_Array *g_scripts;
static JSON_Object *g_scripts_root;
static JSON_Value *g_scripts_value;

static void setup(void)
{
	const char *json =
		"{\"intercept_scripts\":["
		"  {\"func_name\":\"open\","
		"   \"actions\":[{\"action_name\":\"log_params\"}]},"
		"  {\"func_name\":\"*\","
		"   \"actions\":[{\"action_name\":\"log_params\"},"
		"                 {\"action_name\":\"call_real\"}]},"
		"  {\"func_name\":\"close\","
		"   \"actions\":[{\"action_name\":\"log_params\"}]}"
		"]}";

	g_scripts_value = json_parse_string(json);
	g_scripts_root = json_value_get_object(g_scripts_value);
	g_scripts = json_object_get_array(g_scripts_root,
		"intercept_scripts");
}

/* The function under test, exposed via the parson API. We
 * re-declare it here to avoid pulling in the engine's full
 * include chain.
 */
extern const JSON_Object *retrace_script_find(const JSON_Array *i_array,
	const char *func_name,
	void *ret_addr);

struct iter_ctx {
	const char *func_name;
	void *ret_addr;
};

static void bench_op(void *ctx)
{
	struct iter_ctx *c = (struct iter_ctx *)ctx;

	(void)retrace_script_find(g_scripts, c->func_name, c->ret_addr);
}

int main(void)
{
	struct bench_result r;
	struct iter_ctx exact = { "open", NULL };
	struct iter_ctx wildcard = { "nonexistent_fn", NULL };
	struct iter_ctx end_of_array = { "close", NULL };

	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;

	setup();

	printf("--- script_resolver benchmark ---\n");

	if (bench_run("exact_first_index", bench_op, &exact, 100000, &r) == 0)
		bench_print("exact_first_index", &r);

	if (bench_run("wildcard_second_index", bench_op, &wildcard,
		100000, &r) == 0)
		bench_print("wildcard_second_index", &r);

	if (bench_run("end_of_array_third_index", bench_op, &end_of_array,
		100000, &r) == 0)
		bench_print("end_of_array_third_index", &r);

	json_value_free(g_scripts_value);
	return 0;
}
