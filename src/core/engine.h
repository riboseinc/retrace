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

#include "funcs.h"
#include "arch_spec.h"
/* for varags */
#define ENGINE_MAXCOUNT_PARAMS 32

struct ThreadContext {
	const struct FuncPrototype *prototype;

	/* real implementation ptr */
	void *real_impl;

	/* value to set as return value */
	long ret_val;

	struct FuncParam params[ENGINE_MAXCOUNT_PARAMS];

	/* valid param cnt */
	int params_cnt;

	/* set by modify_in_param_{str,int,arr}; consulted by the call_real
	 * action to decide between the trampoline's tail-call path (which
	 * restores x0..x7 AND v0..v7 -- works for FP varargs) and the
	 * C-dispatch path (which only handles integer args -- needed when
	 * any param has been modified, breaks FP varargs).
	 */
	int params_modified;

	/* Reentrance guard for PATH A (trampoline tail-call). When PATH A
	 * is used, real_impl stays set after engine_wrapper returns, so
	 * that calls made BY real_impl (e.g. setlocale calling strcpy)
	 * see the guard and skip full interception. The trampoline calls
	 * retrace_path_a_cleanup after real_impl returns to decrement the
	 * depth and eventually clear the guard.
	 */
	int path_a_depth;

	void *arch_spec_ctx;
	void *ret_addr;
};

extern int retrace_inited;

/* not thread safe */
int retrace_engine_init(void);

void retrace_engine_wrapper(char *func_name, void *arch_spec_ctx);

#endif /* ENGINE_H_ */
