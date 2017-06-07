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
#include <string.h>

#include "common.h"
#include "str.h"

char *
RETRACE_IMPLEMENTATION(strstr)(const char *s1, const char *s2)
{
	real_strstr = RETRACE_GET_REAL(strstr);

	trace_printf(1, "strstr(\"");
	trace_printf_str(s1);
	trace_printf(0, "\", \"");
	trace_printf_str(s2);
	trace_printf(0, "\");\n");

	return real_strstr(s1, s2);
}

RETRACE_REPLACE (strstr)

size_t
RETRACE_IMPLEMENTATION(strlen)(const char *s)
{
	real_strlen = RETRACE_GET_REAL(strlen);

	size_t len = real_strlen(s);

	trace_printf(1, "strlen(\"");
	trace_printf_str(s);
	trace_printf(0, "\"); [len: %zu]\n", len);

	return len;
}

RETRACE_REPLACE (strlen)

int
RETRACE_IMPLEMENTATION(strncmp)(const char *s1, const char *s2, size_t n)
{
	real_strncmp = RETRACE_GET_REAL(strncmp);

	trace_printf(1, "strncmp(\"");
	trace_printf_str(s1);
	trace_printf(0, "\", \"");
	trace_printf_str(s2);
	trace_printf(0, "\", %zu);\n", n);

	return strncmp(s1, s2, n);
}

RETRACE_REPLACE (strncmp)

int
RETRACE_IMPLEMENTATION(strcmp)(const char *s1, const char *s2)
{
	real_strcmp = RETRACE_GET_REAL(strcmp);

	trace_printf(1, "strcmp(\"");
	trace_printf_str(s1);
	trace_printf(0, "\", \"");
	trace_printf_str(s2);
	trace_printf(0, "\");\n");

	return real_strcmp(s1, s2);
}

RETRACE_REPLACE (strcmp)

char *
RETRACE_IMPLEMENTATION(strncpy)(char *s1, const char *s2, size_t n)
{
	size_t len = 0;

	real_strncpy = RETRACE_GET_REAL(strncpy);
	real_strlen = RETRACE_GET_REAL(strlen);

	if (s2)
		len = real_strlen(s2);

	trace_printf(1, "strncpy(%p, \"", s2);
	trace_printf_str(s2);
	trace_printf(0, "\", %zu); [len: %d]\n", n, len);

	return real_strncpy(s1, s2, n);
}

RETRACE_REPLACE (strncpy)

char *
RETRACE_IMPLEMENTATION(strcat)(char *s1, const char *s2)
{
	real_strcat = RETRACE_GET_REAL(strcat);
	real_strlen = RETRACE_GET_REAL(strlen);

	size_t len = real_strlen(s2);

	trace_printf(1, "strcat(%p, \"", s1);
	trace_printf_str(s2);
	trace_printf(0, "\"); [len %zu]\n", len);

	return real_strcat(s1, s2);
}

RETRACE_REPLACE (strcat)

char *
RETRACE_IMPLEMENTATION(strncat)(char *s1, const char *s2, size_t n)
{
	real_strncat = RETRACE_GET_REAL(strncat);
	real_strlen = RETRACE_GET_REAL(strlen);

	size_t len = real_strlen(s2) + 1;

	trace_printf(1, "strncat(%p, \"", s1);
	trace_printf_str(s2);
	trace_printf(0, "\", %zu); [len: %zu]\n", n, len);

	return real_strncat(s1, s2, n);
}

RETRACE_REPLACE (strncat)

char *
RETRACE_IMPLEMENTATION(strcpy)(char *s1, const char *s2)
{
	real_strcpy = RETRACE_GET_REAL(strcpy);
	real_strlen = RETRACE_GET_REAL(strlen);

	size_t len = real_strlen(s2);

	trace_printf(1, "strcpy(%p, \"", s1);
	trace_printf_str(s2);
	trace_printf(0, "\"); [len: %zu]\n", len);

	return real_strcpy(s1, s2);
}

RETRACE_REPLACE (strcpy)
