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
#include <stdarg.h>
#include <stdbool.h>

#include "printf.h"
#include "file.h"

int
RETRACE_IMPLEMENTATION(printf)(const char *fmt, ...)
{
	int result;
	va_list ap;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_PRINTF_FORMAT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fmt, &ap};

	va_start(ap, fmt);

	event_info.function_name = "printf";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);
	va_end(ap);

	va_start(ap, fmt);
	result = real_vprintf(fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	retrace_log_and_redirect_after(&event_info);
	va_end(ap);

	return result;
}

RETRACE_REPLACE_V(printf, int, (const char *fmt, ...), fmt, real_vprintf, (fmt, ap))

int
RETRACE_IMPLEMENTATION(fprintf)(FILE *stream, const char *fmt, ...)
{
	int result;
	va_list ap;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_PRINTF_FORMAT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&stream, &fmt, &ap};

	va_start(ap, fmt);

	event_info.function_name = "fprintf";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);
	va_end(ap);

	va_start(ap, fmt);
	result = real_vfprintf(stream, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	retrace_log_and_redirect_after(&event_info);
	va_end(ap);

	return result;
}

RETRACE_REPLACE_V(fprintf, int, (FILE *stream, const char *fmt, ...), fmt, real_vfprintf, (stream, fmt, ap))

int
RETRACE_IMPLEMENTATION(dprintf)(int fd, const char *fmt, ...)
{
	int result;
	va_list ap;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_PRINTF_FORMAT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fd, &fmt, &ap};

	va_start(ap, fmt);

	event_info.function_name = "dprintf";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);
	va_end(ap);

	va_start(ap, fmt);
	result = real_vdprintf(fd, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	retrace_log_and_redirect_after(&event_info);
	va_end(ap);

	return result;
}

RETRACE_REPLACE_V(dprintf, int, (int fd, const char *fmt, ...), fmt, vdprintf, (fd, fmt, ap))

int
RETRACE_IMPLEMENTATION(sprintf)(char *str, const char *fmt, ...)
{
	int result;
	va_list ap;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING | PARAMETER_FLAG_OUTPUT_VARIABLE, PARAMETER_TYPE_PRINTF_FORMAT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&str, &fmt, &ap};

	va_start(ap, fmt);

	event_info.function_name = "sprintf";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);
	va_end(ap);

	va_start(ap, fmt);
	result = real_vsprintf(str, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	retrace_log_and_redirect_after(&event_info);
	va_end(ap);

	return result;
}

RETRACE_REPLACE_V(sprintf, int, (char *str, const char *fmt, ...), fmt, vsprintf, (str, fmt, ap))

int
RETRACE_IMPLEMENTATION(snprintf)(char *str, size_t size, const char *fmt, ...)
{
	int result;
	va_list ap;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING | PARAMETER_FLAG_OUTPUT_VARIABLE, PARAMETER_TYPE_INT, PARAMETER_TYPE_PRINTF_FORMAT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&str, &size, &fmt, &ap};

	va_start(ap, fmt);

	event_info.function_name = "snprintf";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);
	va_end(ap);

	va_start(ap, fmt);
	result = real_vsnprintf(str, size, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	retrace_log_and_redirect_after(&event_info);
	va_end(ap);

	return result;
}

RETRACE_REPLACE_V(snprintf, int, (char *str, size_t size, const char *fmt, ...), fmt, vsnprintf, (str, size, fmt, ap))

int
RETRACE_IMPLEMENTATION(vprintf)(const char *fmt, va_list ap)
{
	int result;
	va_list ap1;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_PRINTF_FORMAT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fmt, &ap1};

	__va_copy(ap1, ap);

	event_info.function_name = "vprintf";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);
	va_end(ap1);

	__va_copy(ap1, ap);
	result = real_vprintf(fmt, ap);
	va_end(ap1);

	__va_copy(ap1, ap);
	retrace_log_and_redirect_after(&event_info);
	va_end(ap1);

	return result;
}

RETRACE_REPLACE(vprintf, int, (const char *fmt, va_list ap), (fmt, ap))

int
RETRACE_IMPLEMENTATION(vfprintf)(FILE *stream, const char *fmt, va_list ap)
{
	int result;
	va_list ap1;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_PRINTF_FORMAT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&stream, &fmt, &ap1};

	__va_copy(ap1, ap);

	event_info.function_name = "vfprintf";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);
	va_end(ap1);

	__va_copy(ap1, ap);
	result = real_vfprintf(stream, fmt, ap);
	va_end(ap1);

	__va_copy(ap1, ap);
	retrace_log_and_redirect_after(&event_info);
	va_end(ap1);

	return result;
}

RETRACE_REPLACE(vfprintf, int, (FILE *stream, const char *fmt, va_list ap), (stream, fmt, ap))

int
RETRACE_IMPLEMENTATION(vdprintf)(int fd, const char *fmt, va_list ap)
{
	int result;
	va_list ap1;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_PRINTF_FORMAT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fd, &fmt, &ap1};

	__va_copy(ap1, ap);

	event_info.function_name = "vdprintf";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);
	va_end(ap1);

	__va_copy(ap1, ap);
	result = real_vdprintf(fd, fmt, ap);
	va_end(ap1);

	__va_copy(ap1, ap);
	retrace_log_and_redirect_after(&event_info);
	va_end(ap1);

	return result;
}

RETRACE_REPLACE(vdprintf, int, (int fd, const char *fmt, va_list ap), (fd, fmt, ap))

int
RETRACE_IMPLEMENTATION(vsprintf)(char *str, const char *fmt, va_list ap)
{
	int result = 0;
	va_list ap1;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING | PARAMETER_FLAG_OUTPUT_VARIABLE, PARAMETER_TYPE_PRINTF_FORMAT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&str, &fmt, &ap1};

	__va_copy(ap1, ap);

	event_info.function_name = "vsprintf";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);
	va_end(ap1);

	__va_copy(ap1, ap);
	result = real_vsprintf(str, fmt, ap1);
	va_end(ap1);

	__va_copy(ap1, ap);
	retrace_log_and_redirect_after(&event_info);
	va_end(ap1);

	return result;
}

RETRACE_REPLACE(vsprintf, int, (char *str, const char *fmt, va_list ap), (str, fmt, ap))

int
RETRACE_IMPLEMENTATION(vsnprintf)(char *str, size_t size, const char *fmt, va_list ap)
{
	int result;
	va_list ap1;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING | PARAMETER_FLAG_OUTPUT_VARIABLE, PARAMETER_TYPE_INT, PARAMETER_TYPE_PRINTF_FORMAT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&str, &size, &fmt, &ap1};

	__va_copy(ap1, ap);

	event_info.function_name = "vsnprintf";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &result;
	retrace_log_and_redirect_before(&event_info);
	va_end(ap1);

	__va_copy(ap1, ap);
	result = real_vsnprintf(str, size, fmt, ap1);
	va_end(ap1);

	__va_copy(ap1, ap);
	retrace_log_and_redirect_after(&event_info);
	va_end(ap1);

	return result;
}

RETRACE_REPLACE(vsnprintf, int, (char *str, size_t size, const char *fmt, va_list ap), (str, size, fmt, ap))
