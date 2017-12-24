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

enum InterceptActions {
	IA_INVALID,

	/* Do nothing */
	IA_NA,

	/* Log parameters */
	IA_LOG_PARAMS,

	/* Call real implementation */
	IA_CALL_REAL,

	IA_LOG_PARAMS_JSON,

	IA_COUNT
};

enum ArrayCountMethods {
	ACM_INVALID,

	/* Other struct member holds the count */
	ACM_DYN,

	/* Known during compile time */
	ACM_STATIC
};

struct ThreadContext {
	const struct FuncPrototype *prototype;
	void *real_impl;
	long int ret_val;

	/* pointers to the params */
	void *params[MAXCOUNT_PARAMS];

	/* TODO: This should be function specific */
	enum InterceptActions actions[IA_COUNT];

	void *arch_spec_ctx;
};

extern int retrace_inited;

/* not thread safe */
int retrace_engine_init(void);

void retrace_engine_wrapper(char *func_name, void *arch_spec_ctx);

#endif /* ENGINE_H_ */
