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
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "real_impls.h"
#include "arch_spec.h"

struct RetraceRealImpls retrace_real_impls;

/*
 * Windows: the plain CRT/Win32 entry points are the real
 * implementations -- EXCEPT for hooked functions, which must
 * resolve to their relocated TRAMPOLINE (the plain CRT address
 * is the patched bytes once the hook exists; calling it would
 * re-enter the wrapper). DllMain installs hooks BEFORE boot so
 * the trampoline map is already populated here (TODO.windows/05).
 */
#ifdef _WIN32
/*
 * posix_compat.h (via real_impls.h) already pulled windows.h
 * with the macro hygiene undefs.
 */
int retrace_real_impls_init(void)
{
	retrace_real_impls.rc_tss_create = rc_tss_create_win;
	retrace_real_impls.rc_tss_get = rc_tss_get_win;
	retrace_real_impls.rc_tss_set = rc_tss_set_win;
	retrace_real_impls.rc_tss_delete = rc_tss_delete_win;
	retrace_real_impls.rc_mutex_init = rc_mutex_init_win;
	retrace_real_impls.rc_mutex_lock = rc_mutex_lock_win;
	retrace_real_impls.rc_mutex_unlock = rc_mutex_unlock_win;
	retrace_real_impls.rc_mutex_destroy = rc_mutex_destroy_win;
	retrace_real_impls.rc_thread_create = rc_thread_create_win;
	retrace_real_impls.rc_thread_join = rc_thread_join_win;
	retrace_real_impls.free = free;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.dlsym = NULL;
	retrace_real_impls.dlclose = NULL;
	retrace_real_impls.dlopen = NULL;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.strncmp = strncmp;
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.vprintf = vprintf;
	retrace_real_impls.printf = printf;
	retrace_real_impls.fprintf = fprintf;
	retrace_real_impls.fflush = fflush;
	/*
	 * Hookable name: prefer the hook's trampoline. get_real_safe
	 * resolves via ucrtbase/msvcrt GetProcAddress -- under a
	 * STATIC CRT (/MT test binaries) neither module is loaded
	 * and it returns NULL; the compile-time-linked symbol is the
	 * correct real implementation in that build.
	 */
	retrace_real_impls.fopen =
		retrace_as_get_real_safe("fopen");
	if (retrace_real_impls.fopen == NULL)
		retrace_real_impls.fopen = fopen;
	retrace_real_impls.fclose = fclose;
	/*
	 * conf_init reads the config through these; a NULL here is
	 * the v2.13.0 boot segfault (same class as atoi)
	 */
	retrace_real_impls.fseek = fseek;
	retrace_real_impls.ftell = ftell;
	retrace_real_impls.fread = fread;
	retrace_real_impls.real_vsnprintf = vsnprintf;
	retrace_real_impls.time = time;
	retrace_real_impls.atoi = atoi;
	retrace_real_impls.getenv = getenv;
	retrace_real_impls.real_snprintf = snprintf;
	/*
	 * parson's NUMBER serialization goes through this (parson.c
	 * real_sprintf) -- the v2.11.0 "MSVC logger segfault": any
	 * JSON entry with a number crashed on the NULL call.
	 */
	retrace_real_impls.real_sprintf = sprintf;
	return 0;
}

#else /* POSIX: resolve the real symbols below us. */

/* This should be the absolutely the first module to be inited */
int retrace_real_impls_init(void)
{
	retrace_real_impls.dlopen = retrace_as_get_real_safe("dlopen");
	if (retrace_real_impls.dlopen == NULL)
		return -1;

#ifdef __linux__
	/* On glibc < 2.34, pthreads lived in a separate libpthread.so.0 that
	 * the dynamic linker wouldn't load unless explicitly requested.
	 * Loading it made pthread_key_create etc. resolvable via RTLD_NEXT.
	 *
	 * On glibc >= 2.34, libpthread is integrated into libc -- there's no
	 * separate libpthread.so.0 to dlopen. The dlopen call returns NULL,
	 * and that's fine: the symbols are already in libc. Don't fail init
	 * in that case.
	 */
	void *handle;

	handle = retrace_real_impls.dlopen(
			"libpthread.so.0", RTLD_NOW | RTLD_GLOBAL);
	/* handle may be NULL on glibc >= 2.34 -- not an error. */
	(void)handle;
#endif

	retrace_real_impls.rc_tss_create =
		retrace_as_get_real_safe("pthread_key_create");
	if (retrace_real_impls.rc_tss_create == NULL)
		return -3;

	retrace_real_impls.rc_tss_get =
		retrace_as_get_real_safe("pthread_getspecific");
	if (retrace_real_impls.rc_tss_get == NULL)
		return -4;

	retrace_real_impls.rc_tss_set =
		retrace_as_get_real_safe("pthread_setspecific");
	if (retrace_real_impls.rc_tss_set == NULL)
		return -5;

	retrace_real_impls.rc_tss_delete
		= retrace_as_get_real_safe("pthread_key_delete");
	if (retrace_real_impls.rc_tss_delete == NULL)
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

	retrace_real_impls.dlclose = retrace_as_get_real_safe("dlclose");
	/* dlclose may be NULL on some platforms; not fatal */

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

	retrace_real_impls.real_sprintf = retrace_as_get_real_safe("sprintf");
	if (retrace_real_impls.real_sprintf == NULL)
		return -17;

	retrace_real_impls.real_snprintf = retrace_as_get_real_safe("snprintf");
	if (retrace_real_impls.real_snprintf == NULL)
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

	retrace_real_impls.rc_mutex_init = rc_mutex_init_posix;
	if (retrace_real_impls.rc_mutex_init == NULL)
		return -26;

	retrace_real_impls.rc_mutex_lock = rc_mutex_lock_posix;
	if (retrace_real_impls.rc_mutex_lock == NULL)
		return -27;

	retrace_real_impls.rc_mutex_unlock = rc_mutex_unlock_posix;
	if (retrace_real_impls.rc_mutex_unlock == NULL)
		return -28;

	retrace_real_impls.rc_mutex_destroy = rc_mutex_destroy_posix;
	if (retrace_real_impls.rc_mutex_destroy == NULL)
		return -29;

	retrace_real_impls.rc_thread_create = rc_thread_create_posix;
	if (retrace_real_impls.rc_thread_create == NULL)
		return -30;

	retrace_real_impls.rc_thread_join = rc_thread_join_posix;
	if (retrace_real_impls.rc_thread_join == NULL)
		return -31;

	retrace_real_impls.real_vsnprintf =
		retrace_as_get_real_safe("vsnprintf");
	if (retrace_real_impls.real_vsnprintf == NULL)
		return -32;

	retrace_real_impls.time =
		retrace_as_get_real_safe("time");
	if (retrace_real_impls.time == NULL)
		return -33;

	retrace_real_impls.localtime_r =
		retrace_as_get_real_safe("localtime_r");
	if (retrace_real_impls.localtime_r == NULL)
		return -34;

	retrace_real_impls.fprintf =
		retrace_as_get_real_safe("fprintf");
	if (retrace_real_impls.fprintf == NULL)
		return -35;

	retrace_real_impls.fflush =
		retrace_as_get_real_safe("fflush");
	if (retrace_real_impls.fflush == NULL)
		return -36;

	retrace_real_impls.vprintf =
		retrace_as_get_real_safe("vprintf");
	if (retrace_real_impls.vprintf == NULL)
		return -37;

	retrace_real_impls.ctime_r =
		retrace_as_get_real_safe("ctime_r");
	if (retrace_real_impls.ctime_r == NULL)
		return -38;

	return 0;
}

#endif /* _WIN32 / POSIX resolution split */
