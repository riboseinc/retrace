/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>

#include "actions.h"
#include "action_utils.h"
#include "logger.h"
#include "real_impls.h"
#include "data_types.h"

#include "fuzz_dict.h"

static int initialized;

/*
 * Seed policy shared by every fuzz action (TODO.trace-profile/25
 * extracted it so a script using ONLY fuzz_str still seeds):
 * explicit fuzz_seed param > RETRACE_FUZZ_SEED env > time.
 */
static void fuzz_seed_init(const JSON_Object *action_params)
{
	if (initialized)
		return;
	initialized = 1;

	if (action_params != NULL &&
	    json_object_has_value(action_params, "fuzz_seed")) {
		double fuzz_seed;

		fuzz_seed = json_object_get_number(action_params,
			"fuzz_seed");

		srand(fuzz_seed);
	} else if (retrace_real_impls.getenv(
		"RETRACE_FUZZ_SEED") != NULL) {
		/*
		 * External seed (TODO.trace-profile/20): makes
		 * ANY fuzz config deterministically re-drivable
		 * without editing it -- the fuzz workbench's
		 * reproducibility path (a reproducer is config +
		 * this env var).
		 */
		srand((unsigned int)retrace_real_impls.atoi(
			retrace_real_impls.getenv(
				"RETRACE_FUZZ_SEED")));
	} else {
		srand(retrace_real_impls.time(NULL));
	}
}

static int ia_memory_fuzz
	(struct ThreadContext *t_ctx,
		const JSON_Object *action_params)
{
	double fail_rate;
	long random_value;

	if (action_params == NULL) {
		log_err("action_params must exists for modify_return_value_int");
		return -1;
	}

	fuzz_seed_init(action_params);

	/*
	 * If the user has invoked the fuzzing_seed action at any point
	 * (typically at the top of the script), honor it. This makes the
	 * sequence deterministic across multiple memory_fuzz invocations
	 * and across runs. Cheap; only fires when the user explicitly
	 * opted in. See fuzzing_seed.c.
	 */
	retrace_actions_fuzzing_seed_maybe_apply();

	if (!json_object_has_value(action_params, "fail_rate")) {
		log_err("fail_rate must exist in action_params "
			"for memory_fuzz");
		return -1;
	}

	fail_rate = json_object_get_number(action_params, "fail_rate");

	random_value = rand();
	if (random_value <= ((double) RAND_MAX * fail_rate)) {
		errno = ENOMEM;
		t_ctx->ret_val = (long) NULL;

		log_info("Failed memory fuzz");
	} else
		log_info("Passed memory fuzz");

	/* 0 indicates successful processing */
	return 0;
}

/*
 * fuzz_str (TODO.trace-profile/25): dictionary-driven string
 * fuzzing. Replaces an incoming sz param with a token drawn
 * from an AFL-style dictionary file -- deterministic under the
 * shared seed (reproducer = config + seed + dict). match_str
 * optionally gates the replacement to calls carrying a given
 * value, mirroring modify_in_param_str.
 */
static fuzz_dict_t *g_fuzz_str_dict;
static char g_fuzz_str_path[512];

static int ia_fuzz_str
	(struct ThreadContext *t_ctx,
		const JSON_Object *action_params)
{
	const char *param_name;
	const char *dict_path;
	const char *match_str;
	const char *token;
	const struct FuncParam *param;
	int param_idx;

	if (action_params == NULL) {
		log_err("action_params must exists for fuzz_str");
		return -1;
	}

	fuzz_seed_init(action_params);
	retrace_actions_fuzzing_seed_maybe_apply();

	param_name = json_object_get_string(action_params,
			"param_name");

	if (param_name == NULL) {
		log_err("param_name must exist in action_params "
				"for fuzz_str");
		return -1;
	}

	dict_path = json_object_get_string(action_params, "dict");

	if (dict_path == NULL) {
		log_err("dict must exist in action_params "
				"for fuzz_str");
		return -1;
	}

	/* lazy load; a path change reloads (tests swap dicts) */
	if (g_fuzz_str_dict == NULL ||
	    retrace_real_impls.strcmp(g_fuzz_str_path, dict_path) != 0) {
		if (g_fuzz_str_dict == NULL) {
			g_fuzz_str_dict = retrace_real_impls.malloc(
				sizeof(*g_fuzz_str_dict));
			if (g_fuzz_str_dict == NULL) {
				log_err("fuzz_str: out of memory");
				return -1;
			}
		}
		if (fuzz_dict_load(g_fuzz_str_dict, dict_path) != 0) {
			log_err("fuzz_str: cannot load dict '%s'",
				dict_path);
			return -1;
		}
		retrace_real_impls.strcpy(g_fuzz_str_path, dict_path);
	}

	param_idx = retrace_action_find_param(t_ctx, param_name);

	if (param_idx < 0) {
		log_err("param '%s', is not defined for func '%s'",
			param_name, t_ctx->prototype->name);

		return -1;
	}

	param = &t_ctx->params[param_idx];

	if (param->param_meta.direction != PDIR_IN) {
		log_err("param '%s' is not an input param", param_name);

		return -1;
	}

	if (!(param->param_meta.modifiers & CDM_POINTER) ||
		(retrace_real_impls.strcmp(
			param->param_meta.ref_type_name, "sz"))) {
		log_err("param '%s' is not a pointer to string", param_name);

		return -1;
	}

	match_str = json_object_get_string(action_params,
		"match_str");

	if (match_str != NULL) {
		if (retrace_real_impls.strcmp(match_str,
			(char *) param->val)) {
			log_info("no match for param '%s'", param_name);

			/* no match, do nothing */
			return 0;
		}

		log_info("match for param '%s'", param_name);
	}

	token = fuzz_dict_pick(g_fuzz_str_dict);
	if (token == NULL) {
		log_err("fuzz_str: dict '%s' is empty", dict_path);
		return -1;
	}

	if (t_ctx->params[param_idx].free_val) {
		retrace_real_impls.free(
			(void *) t_ctx->params[param_idx].val);
		t_ctx->params[param_idx].free_val = 0;
	}

	t_ctx->params[param_idx].val =
		(intptr_t) retrace_real_impls.malloc(
			retrace_real_impls.strlen(token) + 1);
	t_ctx->params[param_idx].free_val = 1;

	retrace_real_impls.strcpy(
		(char *) t_ctx->params[param_idx].val,
		token);

	log_info("param '%s' fuzzed to '%s'", param_name, token);

	/* 0 indicates successful processing */
	return 0;
}

/*
 * Package name must be unique (it names the external symbol on
 * PE): memfuzz extends the basic set but lives in its own TU.
 */
retrace_actions_define_package(memfuzz) = {
	{
		.name = "memory_fuzz",
		.action = ia_memory_fuzz
	},
	{
		.name = "fuzz_str",
		.action = ia_fuzz_str
	}
};
