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

#ifndef ARCH_SPEC_H_
#define ARCH_SPEC_H_

#include <stdint.h>

#include "arch_spec_macros.h"
#include "data_types.h"
#include "funcs.h"

/*
 * Role-specific registry declarations: each core header defines
 * its role macro next to its only call site (see actions.h,
 * funcs.h, data_types.h). POSIX -> the generic section macro;
 * Windows (RETRACE_WIN_PE_REGISTRY, defined by win_common's
 * arch_spec_macros.h) -> short PE sections walked via the
 * module's own headers (section_walk.c).
 */

struct FuncParam {
	struct ParamMeta param_meta;
	const struct DataType *data_type;
	/*
	 * intptr_t, NOT long: Windows is LLP64 (long = 32 bits) and
	 * val carries POINTERS (log_params derefs, call_real
	 * dispatch). long truncated every pointer arg on MSVC -- the
	 * v2.13/v2.14 "action-path crash". Identity on LP64 POSIX.
	 */
	intptr_t val;
	int free_val;
};

/* this function is an entry point for the hi level logic
 * it will be called from assembly interceptor.
 * must be implemented outside arch_spec
 *
 * retrace_as_sched_real() must be called if invocation of
 * real impl is required
 */
extern void retrace_engine_wrapper(char *func_name,
	void *arch_spec_ctx);

/* schedules real_impl to run after retrace_engine_wrapper */
void retrace_as_sched_real(void *arch_spec_ctx, void *real_impl);
//int retrace_as_sched_real(void *arch_spec_ctx, const char *func_name);

void *retrace_as_get_real_safe(const char *real_impl);

/* cancels real_impl to run after retrace_engine_wrapper */
void retrace_as_cancel_sched_real(void *arch_spec_ctx);

/* should be called by retrace_engine_wrapper to setup params,
 * upon successful completion, *params_cnt will hold number of
 * set up params.
 * Returns 0 in case of failure.
 */
int retrace_as_setup_params(
	void *arch_spec_ctx,
	const struct FuncPrototype *proto,
	struct FuncParam params[],
	int *params_cnt);

/* calls real_impls passing params accordingly to params_meta
 * (intptr_t return: a real impl may return a pointer -- LLP64)
 */
intptr_t retrace_as_call_real(const void *real_impl,
	const struct FuncParam params[],
	int params_cnt);

/* Shared per-arch implementation: plain C function pointer call.
 * Each backend's retrace_as_call_real delegates here. Supports 0..6
 * args; the rare >6-arg case returns -1 (no known libc symbol needs
 * it).
 */
intptr_t retrace_as_call_real_dispatch(const void *real_impl,
	const struct FuncParam params[],
	int params_cnt);

/*
 * Variadic-aware dispatch: casts real_impl to a variadic-typed function
 * pointer so the compiler emits the correct ABI for variadic calls:
 *
 *   - Linux/BSD AArch64 (AAPCS64): variadic args up to x0..x7, then stack
 *   - Apple AArch64: variadic args ALWAYS go on the stack
 *   - x86-64 (System V / Darwin): variadic args in rdi..r9 then stack
 *
 * Without this, the non-variadic dispatch puts variadic args in registers
 * on Apple AArch64; real printf's va_start then reads garbage from the
 * stack (e.g. strlen(NULL) segfault).
 *
 * named_count is proto->params_cnt -- the number of named parameters
 * (printf: 1 for fmt; fprintf-style: 2 for stream+fmt).
 */
intptr_t retrace_as_call_real_variadic(const void *real_impl,
	const struct FuncParam params[],
	int params_cnt,
	int named_count);

/* schedules real_impl to run after retrace_engine_wrapper */
void retrace_as_set_ret_val(void *arch_spec_ctx,
	intptr_t ret_val);

int retrace_as_init(void);

int retrace_as_init_late(void);

#endif /* ARCH_SPEC_H_ */
