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

#ifdef __FreeBSD__
#define _KERNEL
#endif

#include "common.h"
#include "str.h"
#include "printf.h"

#include "env.h"

int RETRACE_IMPLEMENTATION(unsetenv)(const char *name)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&name};
	int r;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "unsetenv";
	event_info.function_group = RTR_FUNC_GRP_SYS;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_unsetenv(name);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(unsetenv, int, (const char *name), (name))

int RETRACE_IMPLEMENTATION(putenv)(char *string)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&string};
	int r;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "putenv";
	event_info.function_group = RTR_FUNC_GRP_SYS;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_putenv(string);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(putenv, int, (char *string), (string))

#define FUZZ_TYPE_BUF_OVERFLOW			"BUFFER_OVERFLOW"
#define FUZZ_TYPE_FORMAT_STR			"FORMAT_STRING"
#define FUZZ_TYPE_USE_GARBAGE			"GARBAGE"

char *RETRACE_IMPLEMENTATION(getenv)(const char *envname)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&envname};
	char *env = NULL;

	char *enabled_vars = NULL;
	char *excluded_vars = NULL;
	char *fuzzing_type_str = NULL;
	int fuzzing_data_len = 0;
	double fail_chance;

	int fuzzing_enabled = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "getenv";
	event_info.function_group = RTR_FUNC_GRP_SYS;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &env;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_config_single("fuzzing-getenv", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_STRING,
		ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT, ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END,
		&enabled_vars, &excluded_vars, &fuzzing_type_str, &fuzzing_data_len, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		int fuzzing_type;
		int reverse;

		char sep[] = "| \t";

		/* check fail flag has set */
		if (fail_chance) {
			/* check list of enabled vars */
			if (real_strcasecmp(enabled_vars, "all") == 0)
				fuzzing_enabled = 1;
			else
				fuzzing_enabled = rtr_check_config_token(envname, enabled_vars, sep, &reverse);

			/* check list of excludin list */
			if (fuzzing_enabled && rtr_check_config_token(envname, excluded_vars, sep, &reverse))
				fuzzing_enabled = 0;
		}

		if (fuzzing_enabled && fuzzing_data_len > 0) {
			if (real_strcmp(fuzzing_type_str, FUZZ_TYPE_BUF_OVERFLOW) == 0)
				fuzzing_type = RTR_FUZZ_TYPE_BUFOVER;
			else if (real_strcmp(fuzzing_type_str, FUZZ_TYPE_FORMAT_STR) == 0)
				fuzzing_type = RTR_FUZZ_TYPE_FMTSTR;
			else if (real_strcmp(fuzzing_type_str, FUZZ_TYPE_USE_GARBAGE) == 0)
				fuzzing_type = RTR_FUZZ_TYPE_GARBAGE;
			else
				fuzzing_enabled = 0;
		} else
			fuzzing_enabled = 0;

		if (fuzzing_enabled) {
			env = (char *) rtr_get_fuzzing_value(fuzzing_type, (void *) &fuzzing_data_len);
			event_info.extra_info = "[redirected]";
			event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
			event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;
		}
	}

	if (!fuzzing_enabled) {
		env = real_getenv(envname);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return (env);
}

RETRACE_REPLACE(getenv, char *, (const char *envname), (envname))

int RETRACE_IMPLEMENTATION(uname)(struct utsname *buf)
{

	int ret;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_UTSNAME, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&buf};


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "uname";
	event_info.function_group = RTR_FUNC_GRP_SYS;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_uname(buf);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(uname, int, (struct utsname *buf), (buf))
