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

#include <string.h>

#include "common.h"
#include "str.h"

char *RETRACE_IMPLEMENTATION(strstr)(const char *s1, const char *s2)
{
	rtr_strstr_t real_strstr;

	real_strstr = RETRACE_GET_REAL(strstr);

	trace_printf(1, "strstr(\"%s\", \"%s\")\n", s1, s2);
	return real_strstr(s1, s2);
}

RETRACE_REPLACE(strstr)

size_t RETRACE_IMPLEMENTATION(strlen)(const char *s)
{
	size_t len;
	rtr_strlen_t real_strlen;

	real_strlen = RETRACE_GET_REAL(strlen);

	len = real_strlen(s);

	if (get_tracing_enabled()) {
		int old_tracing_enabled;

		old_tracing_enabled = set_tracing_enabled(0);
		trace_printf(1, "strlen(\"%s\"); [len: %zu]\n", len);
		set_tracing_enabled(old_tracing_enabled);
	}

	return len;
}

RETRACE_REPLACE(strlen)

int RETRACE_IMPLEMENTATION(strncmp)(const char *s1, const char *s2, size_t n)
{
	rtr_strncmp_t real_strncmp;

	real_strncmp = RETRACE_GET_REAL(strncmp);

	trace_printf(1, "strncmp(\"%s\", \"%s\", %zu)\n", s1, s2, n);
	return real_strncmp(s1, s2, n);
}

RETRACE_REPLACE(strncmp)

int RETRACE_IMPLEMENTATION(strcmp)(const char *s1, const char *s2)
{
	rtr_strcmp_t real_strcmp;
	int old_tracing_enabled;

	real_strcmp = RETRACE_GET_REAL(strcmp);

	if(get_tracing_enabled()) {
		old_tracing_enabled = set_tracing_enabled(0);
		trace_printf(1, "strcmp(\"%s\", \"%s\")\n", s1, s2);
		set_tracing_enabled(old_tracing_enabled);
	}

	return real_strcmp(s1, s2);
}

RETRACE_REPLACE(strcmp)

char *RETRACE_IMPLEMENTATION(strncpy)(char *s1, const char *s2, size_t n)
{
	size_t len = 0;
	rtr_strncpy_t real_strncpy;
	rtr_strlen_t real_strlen;

	real_strncpy	= RETRACE_GET_REAL(strncpy);
	real_strlen	= RETRACE_GET_REAL(strlen);

	len = real_strlen(s2);

	trace_printf(1, "strncpy(%p, \"%s\", %zu); [len: %d]\n", s1, s2, len, n);
	return real_strncpy(s1, s2, n);
}

RETRACE_REPLACE(strncpy)

char *RETRACE_IMPLEMENTATION(strcat)(char *s1, const char *s2)
{
	size_t len;
	rtr_strcat_t real_strcat;
	rtr_strlen_t real_strlen;

	real_strcat	= RETRACE_GET_REAL(strcat);
	real_strlen	= RETRACE_GET_REAL(strlen);

	len = real_strlen(s2);

	trace_printf(1, "strcat(%p, \"%s\"); [len %zu]\n", s1, s2, len);
	return real_strcat(s1, s2);
}

RETRACE_REPLACE(strcat)

char *RETRACE_IMPLEMENTATION(strncat)(char *s1, const char *s2, size_t n)
{
	size_t len;
	rtr_strncat_t real_strncat;
	rtr_strlen_t real_strlen;

	real_strncat	= RETRACE_GET_REAL(strncat);
	real_strlen	= RETRACE_GET_REAL(strlen);

	len = real_strlen(s2) + 1;

	trace_printf(1, "strncat(%p, \"%s\", %zu); [len: %zu]\n", s1, s2, n, len);
	return real_strncat(s1, s2, n);
}

RETRACE_REPLACE(strncat)

char *RETRACE_IMPLEMENTATION(strcpy)(char *s1, const char *s2)
{
	size_t len;
	rtr_strcpy_t real_strcpy;
	rtr_strlen_t real_strlen;

	real_strcpy = RETRACE_GET_REAL(strcpy);
	real_strlen = RETRACE_GET_REAL(strlen);

	len = real_strlen(s2);

	trace_printf(1, "strcpy(%p, \"%s\"); [len: %zu]\n", s1, s2, len);
	return real_strcpy(s1, s2);
}

RETRACE_REPLACE(strcpy)

char *RETRACE_IMPLEMENTATION(strchr)(const char *s, int c)
{
	rtr_strchr_t real_strchr;
	char *result;

	real_strchr = RETRACE_GET_REAL(strchr);

	result = real_strchr(s, c);

	trace_printf(1, "strchr(\"%s\", '%c') [%p]\n", s, c, result);

	return (result);
}

RETRACE_REPLACE(strchr)
