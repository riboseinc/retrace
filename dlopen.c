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
#include "dlopen.h"

void *RETRACE_IMPLEMENTATION(dlopen)(const char *filename, int flag)
{
	rtr_dlopen_t real_dlopen;
	void *r;

	real_dlopen = RETRACE_GET_REAL(dlopen);

	r = real_dlopen(filename, flag);

	trace_printf(1, "dlopen(\"%s\", %d); [return %p]\n", filename, flag, r);

	return r;
}

RETRACE_REPLACE(dlopen)

char *RETRACE_IMPLEMENTATION(dlerror)(void)
{
	rtr_dlerror_t real_dlerror;
	char *r;

	real_dlerror = RETRACE_GET_REAL(dlerror);

	r = real_dlerror();

	trace_printf(1, "dlerror(); [return: \"%s\"]\n", r);

	return r;
}

RETRACE_REPLACE(dlerror)

void *RETRACE_IMPLEMENTATION(dlsym)(void *handle, const char *symbol)
{
	rtr_dlsym_t real_dlsym;
	void *r;

	real_dlsym = RETRACE_GET_REAL(dlsym);

	r = real_dlsym(handle, symbol);

	trace_printf(1, "dlsym(%p, \"%s\"); [return: %p]\n", handle, symbol, r);

	return r;
}

RETRACE_REPLACE(dlsym)

int RETRACE_IMPLEMENTATION(dlclose)(void *handle)
{
	rtr_dlclose_t real_dlclose;
	int r;

	real_dlclose = RETRACE_GET_REAL(dlclose);

	r = real_dlclose(handle);

	trace_printf(1, "dlclose(%p); [return: %d]\n", handle, r);

	return r;
}

RETRACE_REPLACE(dlclose)
