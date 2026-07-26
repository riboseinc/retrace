/*
 * Copyright (c) 2017-2022 [Ribose Inc](https://www.ribose.com).
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
#include "file.h"
#include "char.h"
#include "str.h"

#ifdef __FreeBSD__
#define _DONT_USE_CTYPE_INLINE_
#include <runetype.h>
#undef putc
#endif
#include <ctype.h>

#include <string.h>

int
RETRACE_IMPLEMENTATION(putc)(int c, FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_CHAR, PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&c, &stream};
	int r = 0;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "putc";
	event_info.function_group = RTR_FUNC_GRP_STR;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_putc(c, stream);
	if (r == EOF)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(putc, int, (int c, FILE *stream), (c, stream))


#ifdef HAVE__IO_PUTC
int
RETRACE_IMPLEMENTATION(_IO_putc)(int c, FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_CHAR, PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&c, &stream};
	int r = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "_IO_putc";
	event_info.function_group = RTR_FUNC_GRP_STR;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real__IO_putc(c, stream);
	if (r == EOF)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(_IO_putc, int, (int c, FILE *stream), (c, stream))

#endif

int
RETRACE_IMPLEMENTATION(toupper)(int c)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_CHAR, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&c};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "toupper";
	event_info.function_group = RTR_FUNC_GRP_STR;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_toupper(c);

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(toupper, int, (int c), (c))


int
RETRACE_IMPLEMENTATION(tolower)(int c)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_CHAR, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&c};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "tolower";
	event_info.function_group = RTR_FUNC_GRP_STR;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_tolower(c);

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(tolower, int, (int c), (c))
