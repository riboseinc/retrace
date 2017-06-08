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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdarg.h>

#include "common.h"
#include "printf.h"
#include "file.h"

int
RETRACE_IMPLEMENTATION(printf)(const char *fmt, ...)
{
	char buf[1024];
	rtr_vprintf_t vprintf_ = dlsym(RTLD_NEXT, "vprintf");
	rtr_vsnprintf_t vsnprintf_ = dlsym(RTLD_NEXT, "vsnprintf");

	va_list arglist;

	va_start(arglist, fmt);
	int result = vprintf_(fmt, arglist);
	va_end(arglist);

	va_start(arglist, fmt);
	vsnprintf_(buf, 1024, fmt, arglist);
	va_end(arglist);

	trace_printf(1, "printf(\"");
	trace_printf_str(fmt);
	trace_printf(0, "\" > \"");
	trace_printf_str(buf);
	trace_printf(0, "\")[%d]\n", result);

	return result;
}

int
RETRACE_IMPLEMENTATION(fprintf)(FILE *stream, const char *fmt, ...)
{
	char buf[1024];
	rtr_vfprintf_t vfprintf_ = dlsym(RTLD_NEXT, "vfprintf");
	rtr_vsnprintf_t vsnprintf_ = dlsym(RTLD_NEXT, "vsnprintf");
	rtr_fileno_t fileno_ = dlsym(RTLD_NEXT, "fileno");

	va_list arglist;

	va_start(arglist, fmt);
	int result = vfprintf_(stream, fmt, arglist);
	va_end(arglist);

	va_start(arglist, fmt);
	vsnprintf_(buf, 1024, fmt, arglist);
	va_end(arglist);

	trace_printf(1, "fprintf(\"");
	trace_printf_str(fmt);
	trace_printf(0, "\" > \"");
	trace_printf_str(buf);
	trace_printf(0, "\")[fd=%d][%d]\n", fileno_(stream), result);

	return result;
}

int
RETRACE_IMPLEMENTATION(dprintf)(int fd, const char *fmt, ...)
{
	char buf[1024];
	rtr_vdprintf_t vdprintf_ = dlsym(RTLD_NEXT, "vdprintf");
	rtr_vsnprintf_t vsnprintf_ = dlsym(RTLD_NEXT, "vsnprintf");

	va_list arglist;

	va_start(arglist, fmt);
	int result = vdprintf_(fd, fmt, arglist);
	va_end(arglist);

	va_start(arglist, fmt);
	vsnprintf_(buf, 1024, fmt, arglist);
	va_end(arglist);

	trace_printf(1, "dprintf(\"");
	trace_printf_str(fmt);
	trace_printf(0, "\" > \"");
	trace_printf_str(buf);
	trace_printf(0, "\")[fd=%d][%d]\n", fd, result);

	return result;
}

int
RETRACE_IMPLEMENTATION(sprintf)(char *str, const char *fmt, ...)
{
	char buf[1024];
	rtr_vsprintf_t vsprintf_ = dlsym(RTLD_NEXT, "vsprintf");
	rtr_vsnprintf_t vsnprintf_ = dlsym(RTLD_NEXT, "vsnprintf");

	va_list arglist;

	va_start(arglist, fmt);
	int result = vsprintf_(str, fmt, arglist);
	va_end(arglist);

	va_start(arglist, fmt);
	vsnprintf_(buf, 1024, fmt, arglist);
	va_end(arglist);

	trace_printf(1, "dprintf(\"");
	trace_printf_str(fmt);
	trace_printf(0, "\" > \"");
	trace_printf_str(buf);
	trace_printf(0, "\")[%d]\n", result);

	return result;
}
