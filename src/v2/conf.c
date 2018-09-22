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
			f) != (size_t) fz) {

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

