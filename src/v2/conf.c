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

#if 0

#include <errno.h>

#include "conf.h"
#include "real_impls.h"
#include "logger.h"

#define log_err(fmt, ...) \
	retrace_logger_log(CONF, ERROR, fmt, ##__VA_ARGS__)

#define log_info(fmt, ...) \
	retrace_logger_log(CONF, INFO, fmt, ##__VA_ARGS__)

#define log_warn(fmt, ...) \
	retrace_logger_log(CONF, WARN, fmt, ##__VA_ARGS__)

#define log_dbg(fmt, ...) \
	retrace_logger_log(CONF, DEBUG, fmt, ##__VA_ARGS__)

#define ENVAR_JSON_CONFIG_FN "RETRACE_JSON_CONFIG"

/* Default config - do log_params and call_real for all
 * known functions
 */
char *def_json_conf =
"{"
	"\"intercept_scripts\": ["
		"{"
			"\"func_name\": \"*\","
			"\"actions\": ["
				"{"
					"\"action_name\": \"log_params\""
				"},"
				"{"
					"\"action_name\": \"call_real\""
				"}"
			"]"
		"}"
	"]"
"}";

JSON_Object *retrace_conf;

int retrace_conf_init(void)
{
	const char *conf_fn;
	FILE *f;
	const char *json_conf;
	char *file_json_conf;
	long fz;
	JSON_Value *json_conf_val;

	file_json_conf = NULL;
	f = NULL;

	/* first create an empty conf object */
	retrace_conf = json_value_get_object(
		json_value_init_object());

	/* try environment variable for config json file
	 * do not use json_parse_file_with_comments as it will use C funcs
	 * for io
	 */
	conf_fn = retrace_real_impls.getenv(ENVAR_JSON_CONFIG_FN);
	if (conf_fn != NULL) {
		log_info("config file is set to: '%s'", conf_fn);

		f = retrace_real_impls.fopen(conf_fn, "r");
		if (f == NULL) {
			log_err("fopen failed, errno: %d", errno);
			goto parse_json;
		}

		if (retrace_real_impls.fseek(f,
			0, SEEK_END) == -1) {
			log_err("fseek failed, errno: %d", errno);
			goto parse_json;
		}

		fz = retrace_real_impls.ftell(f);
		if (fz == -1) {
			log_err("ftell failed, errno: %d", errno);
			goto parse_json;
		}

		if (retrace_real_impls.fseek(f,
			0L, SEEK_SET) == -1) {
			log_err("fseek failed, errno: %d", errno);
			goto parse_json;
		}

		file_json_conf = (char *) retrace_real_impls.malloc(fz);

		if (file_json_conf == NULL) {
			log_err("malloc failed, errno: %d", errno);
			goto parse_json;
		}

		if (retrace_real_impls.fread(file_json_conf,
			1,
			fz,
			f) != fz) {

			log_err("fread failed, errno: %d", errno);
			retrace_real_impls.free(file_json_conf);
			file_json_conf = NULL;
		}

		retrace_real_impls.fclose(f);
	} else
		log_warn("config file not set, using the default conf");

parse_json:

	if (file_json_conf != NULL)
		json_conf = file_json_conf;
	else
		json_conf = def_json_conf;

	json_conf_val = json_parse_string_with_comments(
		json_conf);

	if (file_json_conf != NULL)
		retrace_real_impls.free(file_json_conf);

	if (json_conf_val == NULL) {
		log_err("failed to parse json config");
		return -1;
	}

	/* finally set the config root object */
	json_object_clear(retrace_conf);
	retrace_conf = json_value_get_object(json_conf_val);

	return 0;

}

#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <nereon.h>

#include "common.h"

#include "conf.h"

extern rtr2_conf_t *g_rtr2_config;
static nereon_ctx_t g_nereon_ctx;

/*
 * get action list
 */

static int get_actions_list(nereon_config_object_t obj, rtr2_func_t *func)
{
	rtr2_action_t *actions = NULL;
	int actions_count = 0;

	int ret = 0;

	const char *key = "action";

	/* parse action objects */
	while (obj) {
		nereon_object_object_foreach(obj, key, p) {
			rtr2_action_t action;
			char *match_str = NULL, *new_str = NULL;

			nereon_config_option_t action_opts[] = {
				{NULL, NEREON_TYPE_KEY, false, NULL, &action.name},
				{"param_name", NEREON_TYPE_STRING, false, NULL, &action.param_name},
				{"match_type", NEREON_TYPE_INT, false, NULL, &action.match_type},
				{"match_str", NEREON_TYPE_STRING, false, NULL, &match_str},
				{"new_str", NEREON_TYPE_ARRAY, false, NULL, &new_str},
			};

			if (!p)
				break;

			/* get function info from object */
			memset(&action, 0, sizeof(action));
			if (nereon_object_config_options(p, action_opts) != 0) {
				fprintf(stderr, "Failed to get action options(err:%s)\n", nereon_get_errmsg());
				ret = -1;

				break;
			}

			actions = realloc(actions, (actions_count + 1) * sizeof(rtr2_action_t));
			if (!actions) {
				fprintf(stderr, "Out of memory!\n");
				ret = -1;

				break;
			}
			memcpy(&actions[actions_count], &action, sizeof(rtr2_action_t));
			actions_count++;
		}
	}

	if (ret != 0 && actions)
		free(actions);
	else {
		func->actions = actions;
		func->actions_count = actions_count;
	}

	return ret;
}

/*
 * get interception function list
 */

static int get_intercept_funcs_list(nereon_config_object_t obj)
{
	rtr2_func_t *funcs = NULL;
	int funcs_count = 0;

	int ret = 0;

	const char *key = "interception_func";

	/* parse intercept function objects */
	while (obj) {
		nereon_object_object_foreach(obj, key, p) {
			rtr2_func_t func;
			nereon_config_object_t act_objs;

			nereon_config_option_t func_opts[] = {
				{NULL, NEREON_TYPE_KEY, false, NULL, &func.name},
				{"log_level", NEREON_TYPE_STRING, false, NULL, &func.log_level},
				{"action", NEREON_TYPE_ARRAY, false, NULL, &act_objs},
			};

			if (!p)
				break;

			/* get function info from object */
			memset(&func, 0, sizeof(func));
			if (nereon_object_config_options(p, func_opts) != 0) {
				fprintf(stderr, "Failed to get function options(err:%s)\n", nereon_get_errmsg());
				ret = -1;

				break;
			}

			funcs = realloc(funcs, (funcs_count + 1) * sizeof(rtr2_func_t));
			if (!funcs) {
				fprintf(stderr, "Out of memory!\n");
				ret = -1;

				break;
			}
			memcpy(&funcs[funcs_count], &func, sizeof(rtr2_func_t));
			funcs_count++;
		}
	}

	if (ret != 0 && funcs)
		free(funcs);
	else {
		config->funcs = funcs;
		config->funcs_count = funcs_count;
	}

	return ret;
}

/*
 * initialize retrace v2 configuration
 */

int retrace_conf_init(const char *config_fpath)
{
	nereon_config_object_t intercept_funcs = NULL;

	int ret, i;

	nereon_config_option_t rtr2_cfg_opts[] = {
		{"globals.log_path", NEREON_TYPE_STRING, false, NULL, &g_rtr2_config->log_path},
		{"globals.log_level", NEREON_TYPE_STRING, false, NULL, &g_rtr2_config->log_level},
		{"interception_func", NEREON_TYPE_ARRAY, false, NULL, &intercept_funcs}
	};

	g_rtr2_config = (rtr2_conf_t *)malloc(sizeof(rtr2_conf_t));
	if (!g_rtr2_config) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}

	/* initialize nereon context */
	ret = nereon_ctx_init(&g_nereon_ctx, NULL);
	if (ret != 0) {
		fprintf(stderr, "Could not initialize libnereon context(err:%s)\n", nereon_get_errmsg());
		free(g_rtr2_config);
		g_rtr2_config = NULL;

		return -1;
	}

	/* parse configuration file */
	memset(g_rtr2_config, 0, sizeof(rtr2_conf_t));

	fprintf(stderr, "Try to parse configuration file '%s'\n", argv[1]);

	ret = nereon_parse_config_file(&g_nereon_ctx, argv[1]);
	if (ret != 0) {
		fprintf(stderr, "Failed to parse configuration file '%s'(err:%s)\n", argv[1], nereon_get_errmsg());
		goto end;
	}

	/* get configuration options */
	ret = nereon_get_config_options(&g_nereon_ctx, rtr2_cfg_opts);
	if (ret != 0) {
		fprintf(stderr, "Failed to get configuration options(err:%s)\n", nereon_get_errmsg());
		goto end;
	}

	/* get interception function list */
	ret = get_intercept_funcs_list(intercept_funcs);
	if (ret != 0) {
		fprintf(stderr, "Failed to get interception function list\n");
		goto end;
	}

	return 0;

end:
	nereon_ctx_finalize(&g_nereon_ctx);
	free(g_rtr2_config);

	return -1;
}

/*
 * finalize configuration parser
 */

void retrace_conf_finalize(void)
{
	if (!g_rtr2_config)
		return;

	nereon_ctx_finalize(&g_nereon_ctx);
	free(g_rtr2_config);

	g_rtr2_config = NULL;
}

