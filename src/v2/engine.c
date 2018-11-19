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
	int i;
	/* free new params */

	for (i = 0; i != thread_ctx->params_cnt; i++) {
		if (thread_ctx->params[i].free_val)
			retrace_real_impls.free((void *) thread_ctx->params[i].val);
	}

	retrace_real_impls.memset(thread_ctx, 0, sizeof(*thread_ctx));
}

static const JSON_Object *get_i_script(const JSON_Array *i_array,
	const char *func_name,
	void *ret_addr)
{
	size_t i;
	const JSON_Object *i_script;
	const char *i_func;
	double i_ret_addr;
	const JSON_Object *ret_cand;

	ret_cand = NULL;
	for (i = 0; i < json_array_get_count(i_array); i++) {
		i_script = json_array_get_object(i_array, i);

		/* find function name */
		i_func = json_object_get_string(i_script, "func_name");
		if (i_func == NULL) {
			log_err(
				"i_script idx: %d has no func_name member",
				i);
			continue;
		}

		/* check for match-all */
		if (!retrace_real_impls.strcmp(i_func, "*"))
			return i_script;

		/* func_name match? */
		if (!retrace_real_impls.strcmp(i_func, func_name)) {
			if (!ret_addr)
				return i_script;

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
		}
	}

	return ret_cand;
}


struct WrapperSystemVFrame {
	/* this flag will cause the assembly portion to call the real impl */
	long call_real_flag;

	/* In case call_real_flag is 1,
	 * the assembly portion will jmp to this address
	 */
	void *real_impl;

	/* return value for the function,
	 * used in case call_real_flag == 0
	 */
	long ret_val;

	/* original values of the param regs,
	 * as seen by the assembly portion
	 */
	long real_r9;
	long real_r8;
	long real_rcx;
	long real_rdx;
	long real_rsi;
	long real_rdi;
	long real_rsp;
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
	size_t i;
	struct ThreadContext *thread_ctx;
	const JSON_Object *i_script;
	const JSON_Array *i_actions;
	const JSON_Object *i_action;
	int (*action_func)(struct ThreadContext *t_ctx,
		const JSON_Object *action_params);
	const char *i_action_name;
	const JSON_Array *i_scripts;

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


	/* set default to call real impl */
	retrace_as_sched_real(arch_spec_ctx, real_impl);

	/* do not intervene if already intercepting
	 */
	if (thread_ctx->real_impl != NULL)
		return;

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

	/* setup params, do not proceed if failed since
	 * it can be dangerous to call orig with partial params
	 */
	thread_ctx->params_cnt = ENGINE_MAXCOUNT_PARAMS;
	if (!retrace_as_setup_params(thread_ctx->arch_spec_ctx,
		thread_ctx->prototype,
		thread_ctx->params,
		&thread_ctx->params_cnt)) {
		log_err(
			"failed to setup params for %s(), will not proceed",
			func_name);
		goto clean_up;
	}
	/* find intercept script for the func and return addr */
	i_scripts = json_object_get_array(retrace_conf, "intercept_scripts");
	if (!i_scripts) {
		log_warn(
			"%s() config does not contain intercept_scripts",
			func_name);
		goto clean_up;
	}

	i_script = get_i_script(i_scripts, func_name,
		thread_ctx->ret_addr);

	if (!i_script) {
		/* no script defined for this func, call real */
		goto clean_up;
	}

	i_actions = json_object_get_array(i_script, "actions");
	if (i_actions == NULL) {
		log_warn(
			"i_script for %s:%p does not contain actions array",
			func_name,
			thread_ctx->ret_addr);
		goto clean_up;
	}

	if (!json_array_get_count(i_actions)) {
		log_warn(
			"i_script for %s:%p contains empty actions array",
			func_name,
			thread_ctx->ret_addr);
		goto clean_up;
	}

	/* we have script, do not call real impl by default */
	retrace_as_cancel_sched_real(arch_spec_ctx);

	for (i = 0; i < json_array_get_count(i_actions); i++) {

		i_action = json_array_get_object(i_actions, i);
		i_action_name = json_object_get_string(i_action, "action_name");
		if (i_action_name == NULL) {
			log_err(
				"action idx: %d for %s:%p has no action_name "
				"aborting script",
				i,
				func_name,
				thread_ctx->ret_addr);

			break;
		}

		action_func = retrace_actions_get(i_action_name);

		if (!action_func) {
			log_err("action idx: %d for %s:%p "
				"is not supported '%s', aborting script",
				i,
				func_name,
				thread_ctx->ret_addr,
				i_action_name);

			break;
		}

		log_info("Running action %s, for %s:%p, tpid 0x%llx...",
				i_action_name,
				func_name,
				thread_ctx->ret_addr,
				(unsigned long long) pthread_self());

		if (action_func(thread_ctx,
			json_object_get_object(i_action, "action_params"))) {

			log_warn("action %s, for %s:%p, tpid 0x%llx aborted"
				" the script",
				i_action_name,
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

	if (key_res)
		log_err("failed to create pthread_key");

	return key_res;
}
