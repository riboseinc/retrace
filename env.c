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

#ifdef __FreeBSD__
#define _KERNEL
#endif

#include "common.h"
#include "env.h"

int RETRACE_IMPLEMENTATION(unsetenv)(const char *name)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&name};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "unsetenv";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_unsetenv(name);

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
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_putenv(string);

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(putenv, int, (char *string), (string))

char *RETRACE_IMPLEMENTATION(getenv)(const char *envname)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&envname};
	char *env = NULL;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "getenv";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &env;

	retrace_log_and_redirect_before(&event_info);

	env = real_getenv(envname);

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
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	retrace_log_and_redirect_before(&event_info);

	ret = real_uname(buf);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(uname, int, (struct utsname *buf), (buf))
