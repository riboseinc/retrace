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
	void *r;

	r = real_dlopen(filename, flag);

	trace_printf(1, "dlopen(\"%s\", %d); [return %p]\n", filename, flag, r);

	return r;
}

RETRACE_REPLACE(dlopen, void *, (const char *filename, int flag),
	(filename, flag))


char *RETRACE_IMPLEMENTATION(dlerror)(void)
{
	char *r;

	r = real_dlerror();

	trace_printf(1, "dlerror(); [return: \"%s\"]\n", r);

	return r;
}

RETRACE_REPLACE(dlerror, char *, (void), ())


#if !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
#ifdef HAVE_ATOMIC_BUILTINS
void *RETRACE_IMPLEMENTATION(dlsym)(void *handle, const char *symbol)
{
	void *r;

	r = real_dlsym(handle, symbol);

	trace_printf(1, "dlsym(%p, \"%s\"); [return: %p]\n", handle, symbol, r);

	return r;
}

RETRACE_REPLACE(dlsym, void *, (void *handle, const char *symbol),
	(handle, symbol))
#endif
#endif

int RETRACE_IMPLEMENTATION(dlclose)(void *handle)
{
	int r;

	r = real_dlclose(handle);

	trace_printf(1, "dlclose(%p); [return: %d]\n", handle, r);

	return r;
}

RETRACE_REPLACE(dlclose, int, (void *handle), (handle))
