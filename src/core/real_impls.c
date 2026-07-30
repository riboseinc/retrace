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
#include "logger.h"

struct RetraceRealImpls retrace_real_impls;

/*
 * Each entry maps a libc symbol name to its slot in retrace_real_impls.
 * `required` mirrors the original semantics: if the lookup returns NULL
 * for a required entry, init fails with `-(idx + 1)`. Non-required
 * entries (e.g. dlclose on platforms that lack it) leave the slot NULL.
 *
 * Adding a new libc function = appending one row to this table; no
 * engine code changes.
 *
 * NOTE: deliberately NOT `const`. The table holds pointers into
 * retrace_real_impls, which require dynamic relocations at load
 * time. A const table would land in .rodata, where some loaders
 * (musl under qemu emulation, observed on OHOS) refuse to apply
 * relocations — causing a segfault during the constructor.
 */
struct real_impl_init_entry {
	const char *name;
	void **slot;
	int required;
};

#define SLOT(field) .slot = (void **)&retrace_real_impls.field

static struct real_impl_init_entry init_table[] = {
	{SLOT(dlopen),                 .name = "dlopen"},
	{SLOT(pthread_key_create),     .name = "pthread_key_create",     .required = 1},
	{SLOT(pthread_getspecific),    .name = "pthread_getspecific",    .required = 1},
	{SLOT(pthread_setspecific),    .name = "pthread_setspecific",    .required = 1},
	{SLOT(pthread_key_delete),     .name = "pthread_key_delete",     .required = 1},
	{SLOT(free),                   .name = "free",                   .required = 1},
	{SLOT(malloc),                 .name = "malloc",                 .required = 1},
	{SLOT(dlsym),                  .name = "dlsym",                  .required = 1},
	{SLOT(dlclose),                .name = "dlclose"},
	{SLOT(memset),                 .name = "memset",                 .required = 1},
	{SLOT(memcpy),                 .name = "memcpy",                 .required = 1},
	{SLOT(strncmp),                .name = "strncmp",                .required = 1},
	{SLOT(strcmp),                 .name = "strcmp",                 .required = 1},
	{SLOT(strlen),                 .name = "strlen",                 .required = 1},
	{SLOT(strcpy),                 .name = "strcpy",                 .required = 1},
	{SLOT(atoi),                   .name = "atoi",                   .required = 1},
	{SLOT(real_sprintf),           .name = "sprintf",                .required = 1},
	{SLOT(real_snprintf),          .name = "snprintf",               .required = 1},
	{SLOT(getenv),                 .name = "getenv",                 .required = 1},
	{SLOT(fopen),                  .name = "fopen",                  .required = 1},
	{SLOT(fread),                  .name = "fread",                  .required = 1},
	{SLOT(fseek),                  .name = "fseek",                  .required = 1},
	{SLOT(ftell),                  .name = "ftell",                  .required = 1},
	{SLOT(fclose),                 .name = "fclose",                 .required = 1},
	{SLOT(printf),                 .name = "printf",                 .required = 1},
	{SLOT(pthread_mutex_init),     .name = "pthread_mutex_init",     .required = 1},
	{SLOT(pthread_mutex_lock),     .name = "pthread_mutex_lock",     .required = 1},
	{SLOT(pthread_mutex_unlock),   .name = "pthread_mutex_unlock",   .required = 1},
	{SLOT(real_vsnprintf),         .name = "vsnprintf",              .required = 1},
	{SLOT(time),                   .name = "time",                   .required = 1},
	{SLOT(localtime_r),            .name = "localtime_r",            .required = 1},
	{SLOT(fprintf),                .name = "fprintf",                .required = 1},
	{SLOT(fflush),                 .name = "fflush",                 .required = 1},
	{SLOT(vprintf),                .name = "vprintf",                .required = 1},
	{SLOT(ctime_r),                .name = "ctime_r",                .required = 1},
};

/* This should be the absolutely the first module to be inited */
int retrace_real_impls_init(void)
{
	size_t i;
	size_t table_size = sizeof(init_table) / sizeof(init_table[0]);
	void *p;

	/*
	 * dlopen must resolve first: the Linux code path below needs it
	 * to load libpthread.so.0 (glibc < 2.34). The table iteration
	 * afterwards picks up the remaining symbols.
	 */
	retrace_real_impls.dlopen = retrace_as_get_real_safe("dlopen");
	if (retrace_real_impls.dlopen == NULL) {
		log_err("missing required libc symbol 'dlopen'");
		return -1;
	}

#ifdef __linux__
	/*
	 * On glibc < 2.34, pthreads lived in a separate libpthread.so.0
	 * that the dynamic linker wouldn't load unless explicitly requested.
	 * Loading it makes pthread_key_create etc. resolvable via RTLD_NEXT.
	 * On glibc >= 2.34, libpthread is integrated into libc; this
	 * dlopen returns NULL and that's fine.
	 */
	void *handle = retrace_real_impls.dlopen(
		"libpthread.so.0", RTLD_NOW | RTLD_GLOBAL);
	(void)handle;
#endif

	for (i = 0; i < table_size; i++) {
		/* dlopen already resolved above; skip it. */
		if (init_table[i].slot == (void **)&retrace_real_impls.dlopen)
			continue;

		p = retrace_as_get_real_safe(init_table[i].name);
		if (p == NULL) {
			if (init_table[i].required) {
				log_err("missing required libc symbol '%s'",
					init_table[i].name);
				return -((int)i + 1);
			}
			/* non-required: leave the slot NULL */
			continue;
		}
		*init_table[i].slot = p;
	}

	return 0;
}
