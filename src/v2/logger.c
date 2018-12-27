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

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "logger.h"
#include "real_impls.h"

#define ENVAR_LOGGER_DEF_ENA "RETRACE_LOGGER_DEF_ENA"
#define ENVAR_LOGGER_DEF_DECOR_ENA "RETRACE_LOGGER_DEF_DECOR_ENA"
#define ENVAR_LOGGER_DEF_STDOUT_ENA "RETRACE_LOGGER_DEF_STDOUT_ENA"
#define ENVAR_LOGGER_DEF_FN "RETRACE_LOGGER_DEF_FN"

static struct LoggerConfig
{
	int severities_ena[SEVERITY_CNT];
	int modules_ena[MODULES_CNT];
	int ena;
	int decoration_ena;
	int stdout_ena;
	FILE *logfile;

} g_logger_config = {
	.severities_ena = {
		[SEVERITY_DEBUG] = 0,
		[SEVERITY_INFO] = 1,
		[SEVERITY_WARN] = 1,
		[SEVERITY_ERROR] = 1
	},
	.modules_ena = {
		[ACTIONS] = 1,
		[CONF] = 1,
		[DATA_TYPES] = 1,
		[ENGINE] = 1,
		[FUNCS] = 1,
		[MAIN] = 1,
		[ARCH] = 1
	},
	.ena = 1,
	.decoration_ena = 1,
	.stdout_ena = 1,
	.logfile = NULL
};

static char *g_retrace_module_pref[MODULES_CNT] = {"ACT",
	"CONF",
	"TYPES",
	"ENGINE",
	"FUNCS",
	"MAIN",
	"ARCH"
};

static char *g_retrace_severities[SEVERITY_CNT] = {"DEBUG",
	"INFO",
	"WARN",
	"ERROR"
};

static int g_first_json;
static pthread_mutex_t g_logger_mtx;

void retrace_logger_deinit(void)
{
	/* experimental JSON, make array of log messages */
	if (g_logger_config.ena &&
		(g_logger_config.stdout_ena || g_logger_config.logfile)) {

		if (g_logger_config.stdout_ena) {
			retrace_real_impls.printf("]\n");
			retrace_real_impls.fflush(stdout);
		}

		if (g_logger_config.logfile != NULL) {
			retrace_real_impls.fprintf(g_logger_config.logfile, "]\n");
			retrace_real_impls.fflush(g_logger_config.logfile);
		}
	}
/* Dont close logfile it breaks JSON format, the log is flushed anyway */

}

int retrace_logger_init(void)
{
	char *env_val;

	if (retrace_real_impls.pthread_mutex_init(&g_logger_mtx, NULL)) {
		/* cant report error...or use stderr? */
		return 1;
	}

	/* override def config with env parameters */
	env_val =
		retrace_real_impls.getenv(ENVAR_LOGGER_DEF_ENA);
	if (env_val != NULL)
		g_logger_config.ena = retrace_real_impls.atoi(env_val);

	env_val =
		retrace_real_impls.getenv(ENVAR_LOGGER_DEF_DECOR_ENA);
	if (env_val != NULL)
		g_logger_config.decoration_ena =
			retrace_real_impls.atoi(env_val);

	env_val =
		retrace_real_impls.getenv(ENVAR_LOGGER_DEF_STDOUT_ENA);
	if (env_val != NULL)
		g_logger_config.stdout_ena =
			retrace_real_impls.atoi(env_val);

	/* file will be closed upon process termination */
	env_val =
		retrace_real_impls.getenv(ENVAR_LOGGER_DEF_FN);
	if (env_val != NULL)
		g_logger_config.logfile =
			retrace_real_impls.fopen(env_val, "a");

	/* experimental JSON, make array of log messages */
	if (g_logger_config.ena &&
		(g_logger_config.stdout_ena || g_logger_config.logfile)) {

		if (g_logger_config.stdout_ena) {
			retrace_real_impls.printf("[\n");
			retrace_real_impls.fflush(stdout);
		}

		if (g_logger_config.logfile != NULL) {
			retrace_real_impls.fprintf(g_logger_config.logfile, "[\n");
			retrace_real_impls.fflush(g_logger_config.logfile);
		}
	}

	return 0;
}

void retrace_loger_update_config(void)
{
	//const JSON_Object *logger_conf;
	//logger_conf = json_object_get_object(retrace_conf, "logger");
}

void retrace_logger_log_old(int module, int sev, const char *fmt, ...)
{
	va_list args;
	va_list args2;
	size_t pref_size, log_size;
	char *logBuff;
	time_t rawtime;
	struct tm timeinfo;

	if (!(g_logger_config.ena &&
		(g_logger_config.stdout_ena || g_logger_config.logfile)))
		return;

	if (!(g_logger_config.modules_ena[module]
		&& g_logger_config.severities_ena[sev]))
		return;

	if (g_logger_config.decoration_ena) {
		retrace_real_impls.time(&rawtime);
		retrace_real_impls.localtime_r(&rawtime, &timeinfo);

		pref_size = retrace_real_impls.snprintf(NULL, 0, "[RT][%02d:%02d:%02d][%s][%s] ",
			timeinfo.tm_hour,
			timeinfo.tm_min,
			timeinfo.tm_sec,
			g_retrace_module_pref[module],
			g_retrace_severities[sev]);
	} else
		pref_size = 0;

	va_start(args, fmt);
	va_copy(args2, args);

	log_size = retrace_real_impls.vsnprintf(NULL, 0, fmt, args);
	logBuff = (char *) retrace_real_impls.malloc(pref_size + log_size + 1);

	if (g_logger_config.decoration_ena) {
		retrace_real_impls.snprintf(logBuff,
			pref_size + 1,
			"[RT][%02d:%02d:%02d][%s][%s] ",
			timeinfo.tm_hour,
			timeinfo.tm_min,
			timeinfo.tm_sec,
			g_retrace_module_pref[module],
			g_retrace_severities[sev]);
	}

	retrace_real_impls.vsnprintf(logBuff + pref_size, log_size + 1, fmt, args2);

	if (g_logger_config.stdout_ena) {
		retrace_real_impls.printf("%s\n", logBuff);
		retrace_real_impls.fflush(stdout);
	}

	if (g_logger_config.logfile != NULL) {
		retrace_real_impls.fprintf(g_logger_config.logfile,
			"%s\n", logBuff);
		retrace_real_impls.fflush(g_logger_config.logfile);
	}
	retrace_real_impls.free(logBuff);

	va_end(args2);
	va_end(args);
}


void retrace_logger_log(int module, int sev, const char *fmt, ...)
{
	JSON_Value *root_value;
	JSON_Object *root_object;
	va_list args;
	va_list args2;
	size_t log_size;
	char *logBuff;

	root_value = json_value_init_object();
	root_object = json_value_get_object(root_value);

	va_start(args, fmt);
	va_copy(args2, args);

	log_size = retrace_real_impls.vsnprintf(NULL, 0, fmt, args);
	logBuff = (char *) retrace_real_impls.malloc(log_size + 1);

	retrace_real_impls.vsnprintf(logBuff, log_size + 1, fmt, args2);
	json_object_set_string(root_object, "text", logBuff);

	retrace_logger_log_json(module, sev, root_value);

	retrace_real_impls.free(logBuff);
	va_end(args2);
	va_end(args);

	//freed by retrace_logger_log_json
	//json_value_free(root_value);
}

void retrace_logger_log_json(int module, int sev, JSON_Value *msg_value)
{
	time_t rawtime;
	JSON_Value *root_value;
	JSON_Object *root_object;
	char *serialized_string;

	if (!(g_logger_config.ena &&
		(g_logger_config.stdout_ena || g_logger_config.logfile)))
		return;

	if (!(g_logger_config.modules_ena[module]
		&& g_logger_config.severities_ena[sev]))
		return;

	root_value = json_value_init_object();
	root_object = json_value_get_object(root_value);

	char buff[26];

	retrace_real_impls.time(&rawtime);
	retrace_real_impls.ctime_r(&rawtime, buff);

	json_object_set_string(root_object, "time", buff);
	json_object_set_string(root_object, "module",
		g_retrace_module_pref[module]);
	json_object_set_string(root_object, "severity",
			g_retrace_severities[sev]);
	json_object_set_value(root_object, "message", msg_value);

	serialized_string = json_serialize_to_string_pretty(root_value);

	retrace_real_impls.pthread_mutex_lock(&g_logger_mtx);

	if (g_logger_config.stdout_ena) {
		if (g_first_json)
			retrace_real_impls.printf(",\n%s\n", serialized_string);
		else
			retrace_real_impls.printf("%s\n", serialized_string);

		retrace_real_impls.fflush(stdout);
	}

	if (g_logger_config.logfile != NULL) {
		if (g_first_json) {
			retrace_real_impls.fprintf(g_logger_config.logfile,
					",\n%s\n", serialized_string);
		} else {
			retrace_real_impls.fprintf(g_logger_config.logfile,
					"%s\n", serialized_string);
		}

		retrace_real_impls.fflush(g_logger_config.logfile);
	}

	g_first_json = 1;
	retrace_real_impls.pthread_mutex_unlock(&g_logger_mtx);

	json_free_serialized_string(serialized_string);
	json_value_free(root_value);
}
