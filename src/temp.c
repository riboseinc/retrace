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
#include "temp.h"
#include "file.h"

char *RETRACE_IMPLEMENTATION(mktemp)(char *template)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void *parameter_values[] = {&template};
	char *ret = NULL;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "mktemp";
	event_info.function_group = RTR_FUNC_GRP_TEMP;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_mktemp(template);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(mktemp, char *, (char *template), (template))


char *RETRACE_IMPLEMENTATION(mkdtemp)(char *template)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void *parameter_values[] = {&template};
	char *ret = NULL;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "mkdtemp";
	event_info.function_group = RTR_FUNC_GRP_TEMP;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_mktemp(template);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(mkdtemp, char *, (char *template), (template))


int RETRACE_IMPLEMENTATION(mkstemp)(char *template)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void *parameter_values[] = {&template};
	int ret = 0;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "mkstemp";
	event_info.function_group = RTR_FUNC_GRP_TEMP;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_FILE_DESCRIPTOR;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_mkstemp(template);

	if (ret > 0)
		file_descriptor_update(ret, FILE_DESCRIPTOR_TYPE_FILE, template);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(mkstemp, int, (char *template), (template))


int RETRACE_IMPLEMENTATION(mkstemps)(char *template, int suffixlen)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void *parameter_values[] = {&template, &suffixlen};
	int ret = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "mkstemps";
	event_info.function_group = RTR_FUNC_GRP_TEMP;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_mkstemps(template, suffixlen);
	if (ret > 0)
		file_descriptor_update(ret, FILE_DESCRIPTOR_TYPE_FILE, template);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(mkstemps, int, (char *template, int suffixlen), (template, suffixlen))

#ifndef __APPLE__
int RETRACE_IMPLEMENTATION(mkostemp)(char *template, int flags)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void *parameter_values[] = {&template, &flags};
	int ret = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "mkostemp";
	event_info.function_group = RTR_FUNC_GRP_TEMP;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_FILE_DESCRIPTOR;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_mkostemp(template, flags);
	if (ret > 0)
		file_descriptor_update(ret, FILE_DESCRIPTOR_TYPE_FILE, template);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(mkostemp, int, (char *template, int flags), (template, flags))

int RETRACE_IMPLEMENTATION(mkostemps)(char *template, int suffixlen, int flags)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_INT, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void *parameter_values[] = {&template, &suffixlen, &flags};
	int ret = 0;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "mkostemps";
	event_info.function_group = RTR_FUNC_GRP_TEMP;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_mkostemps(template, suffixlen, flags);
	if (ret > 0)
		file_descriptor_update(ret, FILE_DESCRIPTOR_TYPE_FILE, template);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(mkostemps, int, (char *template, int suffixlen, int flags), (template, suffixlen, flags))
#endif

char *RETRACE_IMPLEMENTATION(tempnam)(const char *dir, const char *pfx)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void *parameter_values[] = {&dir, &pfx};
	char *ret = NULL;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "tempnam";
	event_info.function_group = RTR_FUNC_GRP_TEMP;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_tempnam(dir, pfx);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(tempnam, char *, (const char *dir, const char *pfx), (dir, pfx))


FILE *RETRACE_IMPLEMENTATION(tmpfile)(void)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_END};
	FILE *ret = NULL;
	int fd = -1;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "tmpfile";
	event_info.function_group = RTR_FUNC_GRP_TEMP;
	event_info.parameter_types = parameter_types;
	event_info.return_value_type = PARAMETER_TYPE_FILE_STREAM;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_tmpfile();
	if (ret != NULL)
		fd = real_fileno(ret);

	if (fd > 0)
		file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_FILE, "tmpfile() file");

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(tmpfile, FILE *, (void), ())

char *RETRACE_IMPLEMENTATION(tmpnam)(char *s)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING,  PARAMETER_TYPE_END};
	void *parameter_values[] = {&s};
	char *ret = NULL;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "tmpnam";
	event_info.function_group = RTR_FUNC_GRP_TEMP;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_tmpnam(s);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(tmpnam, char *, (char *s), (s))
