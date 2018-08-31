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
#include "conf.h"
#include "logger.h"

#define log_err(fmt, ...) \
	retrace_logger_log(ENGINE, ERROR, fmt, ##__VA_ARGS__)

#define log_info(fmt, ...) \
	retrace_logger_log(ENGINE, INFO, fmt, ##__VA_ARGS__)

#define log_warn(fmt, ...) \
	retrace_logger_log(ENGINE, WARN, fmt, ##__VA_ARGS__)

#define log_dbg(fmt, ...) \
	retrace_logger_log(ENGINE, DEBUG, fmt, ##__VA_ARGS__)

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

		/* reset context data */
		retrace_real_impls.memset(thread_ctx, 0, sizeof(*thread_ctx));

	}

	return thread_ctx;
}

static inline void clear_context(struct ThreadContext *thread_ctx)
{

	/* free new params */
	while (thread_ctx->new_params_cnt >= 0) {
		retrace_real_impls.free(
			thread_ctx->new_params[thread_ctx->new_params_cnt]);
		thread_ctx->new_params_cnt--;
	}

	retrace_real_impls.memset(thread_ctx, 0, sizeof(*thread_ctx));
}

#if 0
const struct FuncPrototype
*retrace_engine_get_func_prototype(
	const char *func_name)
{
	const struct FuncPrototype *p;

	/* TODO: Make a faster search - not O(n) like this */

	/* end of func tables are marked by empty string */
//	p = retrace_funcs;
	while (p->name[0]) {
		if (!retrace_real_impls.strncmp(func_name,
			p->name, MAXLEN_DATATYPE_NAME))
			return p;

		p++;
	}

	return NULL;
}
#endif

static rtr2_func_t *get_i_script(const char *func_name,	void *ret_addr)
{
	int i;

	for (i = 0; i < g_rtr2_config->funcs_count; i++) {
		rtr2_func_t *func = &g_rtr2_config->funcs[i];
		double i_ret_addr;

		/* check for match-all */
		if (!retrace_real_impls.strcmp(func->name, "*"))
			return func;

		/* func_name match? */
		if (!retrace_real_impls.strcmp(func->name, func_name)) {
			if (!ret_addr)
				return func;

/* Fixme: */ 
#if 0
			i_ret_addr = json_object_get_number(i_script, "return_addr");
			if (!i_ret_addr) {
				/* ret_addr not specified */
				if (ret_cand == NULL)
					ret_cand = i_script;
			} else {
				/* ret_addr match? */
				if ((long long) ret_addr == ((long long) i_ret_addr))
					return i_script;
			}
#endif
		}
	}

	return NULL;
}


struct WrapperSystemVFrame {
	/* this flag will cause the assembly portion to call the real impl */
	long int call_real_flag;

	/* In case call_real_flag is 1,
	 * the assembly portion will jmp to this address
	 */
	void *real_impl;

	/* return value for the function,
	 * used in case call_real_flag == 0
	 */
	long int ret_val;

	/* original values of the param regs,
	 * as seen by the assembly portion
	 */
	long int real_r9;
	long int real_r8;
	long int real_rcx;
	long int real_rdx;
	long int real_rsi;
	long int real_rdi;
	long int real_rsp;
};

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
	int (*action_func)(struct ThreadContext *t_ctx,	rtr2_action_t *action);
	const char *i_action_name;

	rtr2_func_t *func = NULL;

	/*
	 * get real implementation
	 * This is very sensitive code, no function calls are allowed.
	 *
	 */
	real_impl = retrace_as_get_real_safe(func_name);
	if (!retrace_inited) {
		/* This can happen if constructor was not
		 * called yet or it failed to initialize
		 * retrace module. For example, on FreeBSD, getenv()
		 * gets called before the constructor.
		 * All we can do is to call the real implementation.
		 *
		 */

		retrace_as_sched_real(arch_spec_ctx, real_impl);
		return;
	}

	/* get/create retrace context for thread */
	thread_ctx = get_thread_context();
	if (thread_ctx == NULL) {
		log_err(
			"%s() intercept failed - could not get context",
			func_name);

		return;
	}

	if (real_impl == NULL) {
		/* cannot process
		 * abort call with return val -1,
		 * -1 is chosen for the best chance of
		 * signaling an error for CRT funcs
		 * The caller will probably crash anyway...
		 */
		retrace_as_set_ret_val(arch_spec_ctx, -1);
		return;
	}

	char *name = (char *) ((struct WrapperSystemVFrame *) arch_spec_ctx)->real_rdi;

	/* set default to call real impl */
	retrace_as_sched_real(arch_spec_ctx, real_impl);

	/* do not intervene if already intercepting
	 */
	if (thread_ctx->real_impl != NULL) {
		return;
	}

	/* save arc spec context */
	thread_ctx->arch_spec_ctx = arch_spec_ctx;

	/* Mark active interception for cases of nested calls */
	thread_ctx->real_impl = real_impl;

	thread_ctx->prototype = retrace_func_get(func_name);

	/* if func is not prototyped - do not intervene */
	if (thread_ctx->prototype == NULL) {
		/* Silently call real impl */
		goto clean_up;
	}

	/* setup params */
	retrace_as_setup_params(thread_ctx->arch_spec_ctx,
		thread_ctx->prototype->params,
		thread_ctx->params);

	char *param_ptr = (char *) thread_ctx->params[0];

	func = get_i_script(func_name, thread_ctx->ret_addr);
	if (!func)
		goto clean_up;

	/* we have script, do not call real impl by default */
	retrace_as_cancel_sched_real(arch_spec_ctx);

	for (i = 0; i < func->actions_count; i++) {
		action_func = retrace_actions_get(func->actions[i].name);
		if (!action_func) {
			log_err("action idx: %d for %s:%p "
				"is not supported '%s', aborting script",
				i,
				func_name,
				thread_ctx->ret_addr,
				func->actions[i].name);

			break;
		}

		log_dbg("Running action %s, for %s:%p, tpid 0x%llx...",
				func->actions[i].name,
				func_name,
				thread_ctx->ret_addr,
				(unsigned long long) pthread_self());

		if (action_func(thread_ctx,	&func->actions[i])) {
			log_warn("action %s, for %s:%p, tpid 0x%llx aborted"
				" the script",
				func->actions[i].name,
				func_name,
				thread_ctx->ret_addr,
				(unsigned long long) pthread_self());
			break;
		}
	}

	/* write back to arch spec. portion */
	retrace_as_set_ret_val(arch_spec_ctx, thread_ctx->ret_val);

clean_up:
	/* mark hi-level intercept done */
	clear_context(thread_ctx);
}

/* not thread safe */
int retrace_engine_init(void)
{
	int key_res;

	key_res = retrace_real_impls.pthread_key_create(&thread_ctx_key,
			thread_ctx_destructor);

	return key_res;
}
