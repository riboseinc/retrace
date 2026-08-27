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

#ifndef ENGINE_H_
#define ENGINE_H_

#include <stddef.h>
#include <stdint.h>

#include "funcs.h"
#include "arch_spec.h"
/* for varags */
#define ENGINE_MAXCOUNT_PARAMS 32

struct ThreadContext {
	const struct FuncPrototype *prototype;

	/* real implementation ptr */
	void *real_impl;

	/* value to set as return value (intptr_t: may be a FILE* --
	 * long is 32-bit on LLP64 Windows)
	 */
	intptr_t ret_val;

	struct FuncParam params[ENGINE_MAXCOUNT_PARAMS];

	/* valid param cnt */
	int params_cnt;

	void *arch_spec_ctx;
	void *ret_addr;
};

extern int retrace_inited;

/*
 * Windows boot (src/core/main.c): DllMain calls this -- MSVC has
 * no constructors and MinGW DLL constructors run after DllMain's
 * hook installation. Idempotent. POSIX uses the constructor.
 */
void retrace_core_boot(void);

/*
 * Perf/test surface: the engine's cached real-impl resolver and
 * its uncached baseline (the historical per-dispatch dlsym).
 * benched head-to-head in test/perf/bench_real_impl_lookup.c.
 */
void *retrace_real_impl_cached(const char *func_name);
void retrace_name_lookup(const char *func_name, void **real_out,
	const struct FuncPrototype **proto_out);
const struct FuncPrototype *retrace_proto_cached(
	const char *func_name);
void *retrace_real_impl_resolve(const char *func_name);

/*
 * dlopen reentrance guard (issue #450). Returns 1 while the current
 * thread is inside a dlopen/dlclose/dlsym/dlerror call. The engine
 * skips action processing while this is active.
 */
int retrace_dlopen_guard_active(void);

/* not thread safe */
int retrace_engine_init(void);

void retrace_engine_wrapper(char *func_name, void *arch_spec_ctx);

#endif /* ENGINE_H_ */
