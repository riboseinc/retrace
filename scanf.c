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
#include <stdbool.h>

#include "common.h"
#include "scanf.h"
#include "printf.h"
#include "file.h"

#define __func__ __extension__ __FUNCTION__

static void
trace(const char *func, bool showfd, int fd, int result, const char *str, const char *fmt, va_list ap)
{
	char buf[1024];

	if (str == NULL) {
		rtr_vsnprintf_t vsnprintf_;

		vsnprintf_ = RETRACE_GET_REAL(vsnprintf);
		vsnprintf_(buf, 1024, fmt, ap);
		str = buf;
	}

	trace_printf(1, "%s(\"", func);
	trace_printf_str(fmt);
	trace_printf(0, "\" > \"");
	trace_printf_str(str);
	if (showfd)
		trace_printf(0, "\")[fd=%d][%d]\n", fd, result);
	else
		trace_printf(0, "\")[%d]\n", result);
}

int
RETRACE_IMPLEMENTATION(scanf)(const char *format, ...)
{
	rtr_vscanf_t real_vscanf;
	va_list ap;
	int result;

	real_vscanf = RETRACE_GET_REAL(vscanf);

	va_start(ap, format);
	result = real_vscanf(format, ap);
	va_end(ap);

	va_start(ap, format);
	trace(__func__, false, 0, result, NULL, format, ap);
	va_end(ap);

	return result;
}

RETRACE_REPLACE(scanf)

int
RETRACE_IMPLEMENTATION(fscanf)(FILE *stream, const char *format, ...)
{
	rtr_vfscanf_t real_vfscanf;
	rtr_fileno_t fileno_;
	va_list ap;
	int result;

	real_vfscanf = RETRACE_GET_REAL(vfscanf);
	fileno_ = RETRACE_GET_REAL(fileno);

	va_start(ap, format);
	result = real_vfscanf(stream, format, ap);
	va_end(ap);

	va_start(ap, format);
	trace(__func__, true, fileno_(stream), result, NULL, format, ap);
	va_end(ap);

	return result;
}

RETRACE_REPLACE(fscanf)

int
RETRACE_IMPLEMENTATION(sscanf)(const char *str, const char *format, ...)
{
	rtr_vsscanf_t real_vsscanf;
	va_list ap;
	int result;

	real_vsscanf = RETRACE_GET_REAL(vsscanf);

	va_start(ap, format);
	result = real_vsscanf(str, format, ap);
	va_end(ap);

	va_start(ap, format);
	trace(__func__, false, 0, result, NULL, format, ap);
	va_end(ap);

	return result;
}

RETRACE_REPLACE(sscanf)

int
RETRACE_IMPLEMENTATION(vscanf)(const char *format, va_list ap)
{
	rtr_vscanf_t real_vscanf;
	va_list ap1;
	int result;

	real_vscanf = RETRACE_GET_REAL(vscanf);

	__va_copy(ap1, ap);
	result = real_vscanf(format, ap);
	trace(__func__, false, 0, result, NULL, format, ap1);
	va_end(ap1);

	return result;
}

RETRACE_REPLACE(vscanf)

int
RETRACE_IMPLEMENTATION(vsscanf)(const char *str, const char *format, va_list ap)
{
	rtr_vsscanf_t real_vsscanf;
	va_list ap1;
	int result;

	real_vsscanf = RETRACE_GET_REAL(vsscanf);

	__va_copy(ap1, ap);
	result = real_vsscanf(str, format, ap);
	trace(__func__, false, 0, result, str, format, ap1);
	va_end(ap1);

	return result;
}

RETRACE_REPLACE(vsscanf)

int
RETRACE_IMPLEMENTATION(vfscanf)(FILE *stream, const char *format, va_list ap)
{
	rtr_vfscanf_t real_vfscanf;
	rtr_fileno_t fileno_;
	va_list ap1;
	int result;

	real_vfscanf = RETRACE_GET_REAL(vfscanf);
	fileno_ = RETRACE_GET_REAL(fileno);

	__va_copy(ap1, ap);
	result = real_vfscanf(stream, format, ap);
	trace(__func__, true, fileno_(stream), result, NULL, format, ap1);
	va_end(ap1);

	return result;
}

RETRACE_REPLACE(vfscanf)
