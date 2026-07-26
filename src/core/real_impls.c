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

#ifdef __linux__
#define _GNU_SOURCE
#include <dlfcn.h>
#endif

#include <stddef.h>

#include "real_impls.h"
#include "arch_spec.h"

struct RetraceRealImpls retrace_real_impls;

/* This should be the absolutely the first module to be inited */
int retrace_real_impls_init(void)
{
	retrace_real_impls.dlopen = retrace_as_get_real_safe("dlopen");
	if (retrace_real_impls.dlopen == NULL)
		return -1;

#ifdef __linux__
	/* load all libs that are used in safe mode,
	 *  since the linker won't link them
	 */
	void *handle;

	handle = retrace_real_impls.dlopen(
			"libpthread.so.0", RTLD_NOW | RTLD_GLOBAL);
	if (handle == NULL)
		return -2;
#endif

	retrace_real_impls.pthread_key_create =
		retrace_as_get_real_safe("pthread_key_create");
	if (retrace_real_impls.pthread_key_create == NULL)
		return -3;

	retrace_real_impls.pthread_getspecific =
		retrace_as_get_real_safe("pthread_getspecific");
	if (retrace_real_impls.pthread_getspecific == NULL)
		return -4;

	retrace_real_impls.pthread_setspecific =
		retrace_as_get_real_safe("pthread_setspecific");
	if (retrace_real_impls.pthread_setspecific == NULL)
		return -5;

	retrace_real_impls.pthread_key_delete
		= retrace_as_get_real_safe("pthread_key_delete");
	if (retrace_real_impls.pthread_key_delete == NULL)
		return -6;

	retrace_real_impls.free = retrace_as_get_real_safe("free");
	if (retrace_real_impls.free == NULL)
		return -7;

	retrace_real_impls.malloc = retrace_as_get_real_safe("malloc");
	if (retrace_real_impls.malloc == NULL)
		return -8;

	retrace_real_impls.dlsym = retrace_as_get_real_safe("dlsym");
	if (retrace_real_impls.dlsym == NULL)
		return -9;

	retrace_real_impls.memset = retrace_as_get_real_safe("memset");
	if (retrace_real_impls.memset == NULL)
		return -10;

	retrace_real_impls.memcpy = retrace_as_get_real_safe("memcpy");
	if (retrace_real_impls.memcpy == NULL)
		return -11;

	retrace_real_impls.strncmp = retrace_as_get_real_safe("strncmp");
	if (retrace_real_impls.strncmp == NULL)
		return -12;

	retrace_real_impls.strcmp = retrace_as_get_real_safe("strcmp");
	if (retrace_real_impls.strcmp == NULL)
		return -13;

	retrace_real_impls.strlen = retrace_as_get_real_safe("strlen");
	if (retrace_real_impls.strlen == NULL)
		return -14;

	retrace_real_impls.strcpy = retrace_as_get_real_safe("strcpy");
	if (retrace_real_impls.strcpy == NULL)
		return -15;

	retrace_real_impls.atoi = retrace_as_get_real_safe("atoi");
	if (retrace_real_impls.atoi == NULL)
		return -16;

	retrace_real_impls.sprintf = retrace_as_get_real_safe("sprintf");
	if (retrace_real_impls.sprintf == NULL)
		return -17;

	retrace_real_impls.snprintf = retrace_as_get_real_safe("snprintf");
	if (retrace_real_impls.snprintf == NULL)
		return -18;

	retrace_real_impls.getenv = retrace_as_get_real_safe("getenv");
	if (retrace_real_impls.getenv == NULL)
		return -19;

	retrace_real_impls.fopen = retrace_as_get_real_safe("fopen");
	if (retrace_real_impls.fopen == NULL)
		return -20;

	retrace_real_impls.fread = retrace_as_get_real_safe("fread");
	if (retrace_real_impls.fread == NULL)
		return -21;

	retrace_real_impls.fseek = retrace_as_get_real_safe("fseek");
	if (retrace_real_impls.fseek == NULL)
		return -22;

	retrace_real_impls.ftell = retrace_as_get_real_safe("ftell");
	if (retrace_real_impls.ftell == NULL)
		return -23;

	retrace_real_impls.fclose = retrace_as_get_real_safe("fclose");
	if (retrace_real_impls.fclose == NULL)
		return -24;

	retrace_real_impls.printf = retrace_as_get_real_safe("printf");
	if (retrace_real_impls.printf == NULL)
		return -25;

	retrace_real_impls.pthread_mutex_init =
		retrace_as_get_real_safe("pthread_mutex_init");
	if (retrace_real_impls.pthread_mutex_init == NULL)
		return -26;

	retrace_real_impls.pthread_mutex_lock =
		retrace_as_get_real_safe("pthread_mutex_lock");
	if (retrace_real_impls.pthread_mutex_lock == NULL)
		return -27;

	retrace_real_impls.pthread_mutex_unlock =
		retrace_as_get_real_safe("pthread_mutex_unlock");
	if (retrace_real_impls.pthread_mutex_unlock == NULL)
		return -28;

	retrace_real_impls.vsnprintf =
		retrace_as_get_real_safe("vsnprintf");
	if (retrace_real_impls.vsnprintf == NULL)
		return -29;

	retrace_real_impls.time =
		retrace_as_get_real_safe("time");
	if (retrace_real_impls.time == NULL)
		return -30;

	retrace_real_impls.localtime_r =
		retrace_as_get_real_safe("localtime_r");
	if (retrace_real_impls.localtime_r == NULL)
		return -31;

	retrace_real_impls.fprintf =
		retrace_as_get_real_safe("fprintf");
	if (retrace_real_impls.fprintf == NULL)
		return -32;

	retrace_real_impls.fflush =
		retrace_as_get_real_safe("fflush");
	if (retrace_real_impls.fflush == NULL)
		return -33;

	retrace_real_impls.vprintf =
		retrace_as_get_real_safe("vprintf");
	if (retrace_real_impls.vprintf == NULL)
		return -34;

	retrace_real_impls.ctime_r =
		retrace_as_get_real_safe("ctime_r");
	if (retrace_real_impls.ctime_r == NULL)
		return -35;

	return 0;
}
