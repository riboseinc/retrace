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

#ifndef __RETRACE_PRINT_H__
#define __RETRACE_PRINT_H__
typedef int (*rtr_printf_t)(const char *fmt, ...);
typedef int (*rtr_fprintf_t)(FILE *stream, const char *fmt, ...);
typedef int (*rtr_dprintf_t)(int fd, const char *fmt, ...);
typedef int (*rtr_sprintf_t)(char *str, const char *fmt, ...);
typedef int (*rtr_snprintf_t)(char *str, size_t size, const char *fmt, ...);
typedef int (*rtr_vprintf_t)(const char *fmt, va_list ap);
typedef int (*rtr_vfprintf_t)(FILE *stream, const char *fmt, va_list ap);
typedef int (*rtr_vdprintf_t)(int fd, const char *fmt, va_list ap);
typedef int (*rtr_vsprintf_t)(char *str, const char *fmt, va_list ap);
typedef int (*rtr_vsnprintf_t)(char *buf, size_t size, const char *fmt, va_list ap);

rtr_vsnprintf_t	real_vsnprintf;
rtr_fprintf_t	real_fprintf;
rtr_printf_t	real_printf;
#endif