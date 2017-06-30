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

#include "common.h"
#include <string.h>

#include "str.h"

char *RETRACE_IMPLEMENTATION(strstr)(const char *s1, const char *s2)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&s1, &s2};
	char *result = NULL;



	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "strstr";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);

	result = real_strstr(s1, s2);

	retrace_log_and_redirect_after(&event_info);

	return (result);
}

RETRACE_REPLACE(strstr, char *, (const char *s1, const char *s2), (s1, s2))

size_t RETRACE_IMPLEMENTATION(strlen)(const char *s)
{
	size_t len;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&s};


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "strlen";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &len;
	retrace_log_and_redirect_before(&event_info);

	len = real_strlen(s);

	retrace_log_and_redirect_after(&event_info);

	return len;
}

RETRACE_REPLACE(strlen, size_t, (const char *s), (s))

int RETRACE_IMPLEMENTATION(strncmp)(const char *s1, const char *s2, size_t n)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING_LEN, PARAMETER_TYPE_STRING_LEN, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&n, &s1, &n, &s2, &n};
	int result;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "strncmp";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);

	result = real_strncmp(s1, s2, n);

	retrace_log_and_redirect_after(&event_info);

	return (result);
}

RETRACE_REPLACE(strncmp, int, (const char *s1, const char *s2, size_t n), (s1, s2, n))

int RETRACE_IMPLEMENTATION(strcmp)(const char *s1, const char *s2)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&s1, &s2};
	int result;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "strcmp";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);

	result = real_strcmp(s1, s2);

	retrace_log_and_redirect_after(&event_info);

	return (result);
}

RETRACE_REPLACE(strcmp, int, (const char *s1, const char *s2), (s1, s2))

char *RETRACE_IMPLEMENTATION(strncpy)(char *s1, const char *s2, size_t n)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING_LEN, PARAMETER_TYPE_STRING_LEN, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&n, &s1, &n, &s2, &n};
	char *result = NULL;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "strncpy";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);

	result = real_strncpy(s1, s2, n);

	retrace_log_and_redirect_after(&event_info);

	return (result);
}

RETRACE_REPLACE(strncpy, char *, (char *s1, const char *s2, size_t n), (s1, s2, n))

char *RETRACE_IMPLEMENTATION(strcat)(char *s1, const char *s2)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&s1, &s2};
	char *result = NULL;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "strcat";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);

	result = real_strcat(s1, s2);

	retrace_log_and_redirect_after(&event_info);

	return (result);
}

RETRACE_REPLACE(strcat, char *, (char *s1, const char *s2), (s1, s2))

char *RETRACE_IMPLEMENTATION(strncat)(char *s1, const char *s2, size_t n)
{
	char *result = NULL;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&s1, &s2, &n};


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "strncat";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);

	result = real_strncat(s1, s2, n);

	retrace_log_and_redirect_after(&event_info);

	return (result);
}

RETRACE_REPLACE(strncat, char *, (char *s1, const char *s2, size_t n), (s1, s2, n))

char *RETRACE_IMPLEMENTATION(strcpy)(char *s1, const char *s2)
{
	char *result = NULL;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&s1, &s2};


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "strcpy";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);

	result = real_strcpy(s1, s2);

	retrace_log_and_redirect_after(&event_info);

	return (result);
}

RETRACE_REPLACE(strcpy, char *, (char *s1, const char *s2), (s1, s2))

char *RETRACE_IMPLEMENTATION(strchr)(const char *s, int c)
{
	char *result = NULL;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_CHAR, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&s, &c};


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "strchr";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);

	result = real_strchr(s, c);

	retrace_log_and_redirect_after(&event_info);

	return (result);
}

RETRACE_REPLACE(strchr, char *, (const char *s, int c), (s, c))
