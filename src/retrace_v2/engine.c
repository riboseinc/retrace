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


#ifndef __GNUC__
#error GNU extensions are required!
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <dlfcn.h>

#include "engine.h"
#include "real_impls.h"
#include "arch_spec.h"
#include "actions.h"

#define retrace_engine_warn(fmt, ...) \
	printf("[WARN] " fmt "\n", ## __VA_ARGS__)

#define retrace_engine_info(fmt, ...) \
	printf("[INFO] " fmt "\n", ## __VA_ARGS__)

#define retrace_engine_error(fmt, ...) \
	printf("[ERROR]" fmt "\n", ## __VA_ARGS__)

static pthread_key_t thread_ctx_key;

static void thread_ctx_destructor(void *thread_ctx)
{
	retrace_real_impls.free(thread_ctx);
	retrace_real_impls.pthread_key_delete(thread_ctx_key);
}
static inline struct ThreadContext *get_thread_context(void)
{
	struct ThreadContext *thread_ctx;

	thread_ctx = (struct ThreadContext *)
		retrace_real_impls.pthread_getspecific(thread_ctx_key);

	/* try to create if does not exist */
	if (thread_ctx == NULL) {

		thread_ctx = (struct ThreadContext *)
			retrace_real_impls.malloc(sizeof(struct ThreadContext));

		if (thread_ctx == NULL)
			return NULL;

		if (retrace_real_impls.pthread_setspecific(
			thread_ctx_key, thread_ctx)) {

			retrace_real_impls.free(thread_ctx);
			return NULL;
		}

		/* TODO: init thread_ctx */
		retrace_real_impls.memset(thread_ctx, 0, sizeof(*thread_ctx));

		/* set default actions */
		thread_ctx->actions[0] = IA_LOG_PARAMS;
		thread_ctx->actions[1] = IA_CALL_REAL;
		thread_ctx->actions[2] = IA_INVALID;
	}

	return thread_ctx;
}

const struct FuncPrototype
*retrace_engine_get_func_prototype(
	const char *func_name)
{
	const struct FuncPrototype *p;

	/* TODO: Make a faster search - not O(n) like this */

	/* end of func tables are marked by empty string */
	p = retrace_funcs;
	while (p->name[0]) {
		if (!retrace_real_impls.strncmp(func_name,
			p->name, MAXLEN_DATATYPE_NAME))
			return p;

		p++;
	}

	return NULL;
}

/* The purpose of this function is to continue the work of the assembly wrapper.
 * The separation exists to allow for an easy support of different archs/ABIs
 * Assembly wrapper should call this function as soon as it saves
 * original parameters to its context
 */
void retrace_engine_wrapper(char *func_name,
		void *arch_spec_ctx)
{
	void *real_impl;
	int i;
	struct ThreadContext *thread_ctx;

	if (!retrace_inited) {
		/* This can happen if constructor was not
		 * called yet or it failed to initialize
		 * retrace module. For example, on FreeBSD, getenv()
		 * gets called before the constructor.
		 * All we can do is to call the real implementation
		 */

		/* FIXME: calling dlsym directly!
		 * will crash if dlsym is a interceptable itself
		 */
		real_impl = dlsym(RTLD_NEXT, func_name);

		retrace_as_sched_real(arch_spec_ctx, real_impl);
		return;
	}
	/* get/create retrace context for thread */
	thread_ctx = get_thread_context();

	/* get real implementation */
	//real_impl = retrace_engine_get_real_impl_safe(func_name);
	real_impl = retrace_real_impls.dlsym(RTLD_NEXT, func_name);

	if (thread_ctx == NULL ||
			real_impl == NULL) {
		/* cannot process */

		if (real_impl == NULL)
			/* abort call with return val -1,
			 * -1 is chosen for the best chance of
			 * signaling an error for CRT funcs
			 * The caller will probably crash anyway...
			 */
			retrace_as_abort(arch_spec_ctx, -1);
		else
			retrace_as_sched_real(arch_spec_ctx, real_impl);

		return;
	}

	/* Up to this point only safe funcs can be used */
	asm volatile ("":::"memory");

	/* do not intervene if already intercepting */
	if (thread_ctx->real_impl != NULL) {
		retrace_as_sched_real(arch_spec_ctx, real_impl);
		return;
	}

	/* save arc spec context */
	thread_ctx->arch_spec_ctx = arch_spec_ctx;

	/* Mark active interception for cases of nested calls */
	thread_ctx->real_impl = real_impl;

	thread_ctx->prototype =
		retrace_engine_get_func_prototype(func_name);

	/* if func is not prototyped - do not intervene */
	if (thread_ctx->prototype == NULL) {
		retrace_engine_warn(
			"%s() is intercepted but not prototyped, will not intervene",
			func_name);

		retrace_as_sched_real(arch_spec_ctx, real_impl);
		goto end_intercept;
	}

	/* setup params */
	retrace_as_setup_params(thread_ctx->arch_spec_ctx,
		thread_ctx->prototype->params,
		thread_ctx->params);

	/* run the script */
	i = 0;
	while (thread_ctx->actions[i] != IA_INVALID) {
		retrace_engine_info("Running action %d, for %s(), tpid 0x%llx...",
				thread_ctx->actions[i],
				func_name,
				(unsigned long long) pthread_self());

		if (!retrace_intercept_actions[thread_ctx->actions[i]](
			thread_ctx))
			break;

		i++;
	}

end_intercept:
	thread_ctx->real_impl = NULL;

	retrace_as_intercept_done(thread_ctx->arch_spec_ctx,
		thread_ctx->ret_val);
}

/* not thread safe */
int retrace_engine_init(void)
{
	int key_res;

	key_res = retrace_real_impls.pthread_key_create(&thread_ctx_key,
			thread_ctx_destructor);

	return key_res;
}
