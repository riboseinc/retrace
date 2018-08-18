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

#include "arch_spec_macros.h"
#include "data_types.h"
#include "funcs.h"

struct FuncParam {
	struct ParamMeta param_meta;
	const struct DataType *data_type;
	long int val;
	int free_val;
};

/*
void retrace_as_abort(void *arch_spec_ctx, long int ret_val);
*/

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

/* should be called by retrace_engine_wrapper to setup params */
int retrace_as_setup_params(
	void *arch_spec_ctx,
	const struct FuncPrototype *proto,
	struct FuncParam params[],
	int *params_cnt);

/* calls real_impls passing params accordingly to params_meta */
long int retrace_as_call_real(const void *real_impl,
	const struct FuncParam params[],
	int params_cnt);

/* schedules real_impl to run after retrace_engine_wrapper */
void retrace_as_set_ret_val(void *arch_spec_ctx,
	long int ret_val);

int retrace_as_init(void);

#endif /* ARCH_SPEC_H_ */
