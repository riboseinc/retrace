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
#include <stdbool.h>

#include "common.h"
#include "printf.h"
#include "file.h"

static void
trace(const char *func, bool showfd, int fd, int result,
	  const char *str, const char *fmt, va_list ap)
{
	char buf[1024];

	if (str == NULL) {
		rtr_vsnprintf_t vsnprintf_ = dlsym(RTLD_NEXT, "vsnprintf");
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
RETRACE_IMPLEMENTATION(printf)(const char *fmt, ...)
{
	rtr_vprintf_t vprintf_ = dlsym(RTLD_NEXT, "vprintf");
	va_list ap;

	va_start(ap, fmt);
	int result = vprintf_(fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	trace(__func__, false, 0, result, NULL, fmt, ap);
	va_end(ap);

	return result;
}

int
RETRACE_IMPLEMENTATION(fprintf)(FILE *stream, const char *fmt, ...)
{
	rtr_vfprintf_t vfprintf_ = dlsym(RTLD_NEXT, "vfprintf");
	rtr_fileno_t fileno_ = dlsym(RTLD_NEXT, "fileno");
	va_list ap;

	va_start(ap, fmt);
	int result = vfprintf_(stream, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	trace(__func__, true, fileno_(stream), result, NULL, fmt, ap);
	va_end(ap);

	return result;
}

int
RETRACE_IMPLEMENTATION(dprintf)(int fd, const char *fmt, ...)
{
	rtr_vdprintf_t vdprintf_ = dlsym(RTLD_NEXT, "vdprintf");
	va_list ap;

	va_start(ap, fmt);
	int result = vdprintf_(fd, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	trace(__func__, true, fd, result, NULL, fmt, ap);
	va_end(ap);

	return result;
}

int
RETRACE_IMPLEMENTATION(sprintf)(char *str, const char *fmt, ...)
{
	rtr_vsprintf_t vsprintf_ = dlsym(RTLD_NEXT, "vsprintf");
	va_list ap;

	va_start(ap, fmt);
	int result = vsprintf_(str, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	trace(__func__, false, 0, result, NULL, fmt, ap);
	va_end(ap);

	return result;
}

int
RETRACE_IMPLEMENTATION(snprintf)(char *str, size_t size, const char *fmt, ...)
{
	rtr_vsnprintf_t vsnprintf_ = dlsym(RTLD_NEXT, "vsnprintf");
	va_list ap;

	va_start(ap, fmt);
	int result = vsnprintf_(str, size, fmt, ap);
	va_end(ap);

	trace(__func__, false, 0, result, str, fmt, ap);

	return result;
}

int
RETRACE_IMPLEMENTATION(vprintf)(const char *fmt, va_list ap)
{
	rtr_vprintf_t vprintf_ = dlsym(RTLD_NEXT, "vprintf");
	va_list ap1;

	va_copy(ap1, ap);
	int result = vprintf_(fmt, ap);
	trace(__func__, false, 0, result, NULL, fmt, ap1);
	va_end(ap1);

	return result;
}

int
RETRACE_IMPLEMENTATION(vfprintf)(FILE *stream, const char *fmt, va_list ap)
{
	rtr_vfprintf_t vfprintf_ = dlsym(RTLD_NEXT, "vfprintf");
	rtr_fileno_t fileno_ = dlsym(RTLD_NEXT, "fileno");
	va_list ap1;

	va_copy(ap1, ap);
	int result = vfprintf_(stream, fmt, ap);
	trace(__func__, true, fileno_(stream), result, NULL, fmt, ap1);
	va_end(ap1);

	return result;
}

int
RETRACE_IMPLEMENTATION(vdprintf)(int fd, const char *fmt, va_list ap)
{
	rtr_vdprintf_t vdprintf_ = dlsym(RTLD_NEXT, "vdprintf");
	va_list ap1;

	va_copy(ap1, ap);
	int result = vdprintf_(fd, fmt, ap);
	trace(__func__, true, fd, result, NULL, fmt, ap1);
	va_end(ap1);

	return result;
}

int
RETRACE_IMPLEMENTATION(vsprintf)(char *str, const char *fmt, va_list ap)
{
	rtr_vsprintf_t vsprintf_ = dlsym(RTLD_NEXT, "vsprintf");
	va_list ap1;

	va_copy(ap1, ap);
	int result = vsprintf_(str, fmt, ap);
	trace(__func__, false, 0, result, NULL, fmt, ap1);
	va_end(ap1);

	return result;
}

int
RETRACE_IMPLEMENTATION(vsnprintf)(char *str, size_t size, const char *fmt, va_list ap)
{
	rtr_vsnprintf_t vsnprintf_ = dlsym(RTLD_NEXT, "vsnprintf");

	int result = vsnprintf_(str, size, fmt, ap);

	trace(__func__, false, 0, result, str, fmt, ap);

	return result;
}
