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

/* for RTLD_NEXT, may not fork on other platforms */

#ifndef __GNUC__
#error GNU extensions are required!
#endif

#define _GNU_SOURCE
#include <dlfcn.h>

#include "real_impls.h"

struct RetraceRealImpls retrace_real_impls = {0};
void *__libc_dlsym(void *__map, const char *__name);
void *_dl_sym(void *handle, const char *name, void *dl_caller);

static inline void *real_impls_safe(const char *func_name)
{
	/* This assumes there will be no interceptions for __libc_dlsym */
	//return __libc_dlsym(RTLD_NEXT, func_name);
	//return _dl_sym(RTLD_NEXT, func_name, __func__);

	//TODO: Fix the above code so it wont be intercepted
	// currently dlsym can be intercepted
	return dlsym(RTLD_DEFAULT, func_name);
}

int retrace_real_impls_init_safe(void)
{
	void *handle;

	retrace_real_impls.dlopen = real_impls_safe("dlopen");
	if (retrace_real_impls.dlopen == NULL)
		return -1;

#ifdef __linux__
	/* load all libs that are used in safe mode,
	 *  since the linker wont link them
	 */
	handle = retrace_real_impls.dlopen(
			"libpthread.so.0", RTLD_NOW | RTLD_GLOBAL);
	if (handle == NULL)
		return -2;
#endif

	retrace_real_impls.pthread_key_create =
		real_impls_safe("pthread_key_create");
	if (retrace_real_impls.pthread_key_create == NULL)
		return -3;

	retrace_real_impls.pthread_getspecific =
		real_impls_safe("pthread_getspecific");
	if (retrace_real_impls.pthread_getspecific == NULL)
		return -4;

	retrace_real_impls.pthread_setspecific =
		real_impls_safe("pthread_setspecific");
	if (retrace_real_impls.pthread_setspecific == NULL)
		return -5;

	retrace_real_impls.pthread_key_delete
		= real_impls_safe("pthread_key_delete");
	if (retrace_real_impls.pthread_key_delete == NULL)
		return -6;

	retrace_real_impls.free = real_impls_safe("free");
	if (retrace_real_impls.free == NULL)
		return -7;

	retrace_real_impls.malloc = real_impls_safe("malloc");
	if (retrace_real_impls.malloc == NULL)
		return -8;

	retrace_real_impls.dlsym = real_impls_safe("dlsym");
	if (retrace_real_impls.dlsym == NULL)
		return -9;

	retrace_real_impls.memset = real_impls_safe("memset");
	if (retrace_real_impls.memset == NULL)
		return -10;

	retrace_real_impls.memcpy = real_impls_safe("memcpy");
	if (retrace_real_impls.memcpy == NULL)
		return -11;

	retrace_real_impls.strncmp = real_impls_safe("strncmp");
	if (retrace_real_impls.strncmp == NULL)
		return -12;

	retrace_real_impls.strcmp = real_impls_safe("strcmp");
	if (retrace_real_impls.strcmp == NULL)
		return -13;

	retrace_real_impls.strlen = real_impls_safe("strlen");
	if (retrace_real_impls.strlen == NULL)
		return -14;

	retrace_real_impls.strcpy = real_impls_safe("strcpy");
	if (retrace_real_impls.strcpy == NULL)
		return -15;

	retrace_real_impls.atoi = real_impls_safe("atoi");
	if (retrace_real_impls.atoi == NULL)
		return -16;

	retrace_real_impls.sprintf = real_impls_safe("sprintf");
	if (retrace_real_impls.sprintf == NULL)
		return -17;

	retrace_real_impls.snprintf = real_impls_safe("snprintf");
	if (retrace_real_impls.snprintf == NULL)
		return -18;

	return 0;
}

