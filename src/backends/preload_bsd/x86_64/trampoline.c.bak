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

#define _GNU_SOURCE
#include <dlfcn.h>

#include <printf.h>
#include <pthread.h>

#include "engine.h"
#include "real_impls.h"
#include "logger.h"

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

struct AsThreadContext {
	/*
	 * this is to emulate linux printf mechanics, 0 means unknown type
	 * values are PA_* or 0 for unknown
	 */
	int printf_args_types[ENGINE_MAXCOUNT_PARAMS];
	int printf_args_cnt;
};
static pthread_key_t as_thread_ctx_key;
static pthread_mutex_t as_printf_mtx;

static void as_thread_ctx_destructor(void *thread_ctx)
{
	if (thread_ctx) {
		log_err("bug! orphaned thread context");
		retrace_real_impls.free(thread_ctx);
	}

	retrace_real_impls.pthread_key_delete(as_thread_ctx_key);
}

static int as_arginfo_function(const struct printf_info *__info,
	     size_t __n, int *__argtypes)
{
	struct AsThreadContext *ctx;

	ctx = (struct AsThreadContext *)
		retrace_real_impls.pthread_getspecific(as_thread_ctx_key);

	if (ctx == NULL)
		return 0;

	switch (__info->spec) {
	case 'p':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_POINTER;
		break;

	case 's':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_STRING;
		break;

	case 'S':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_WSTRING;
		break;

	case 'g':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_DOUBLE;
		break;

	case 'G':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_DOUBLE;
		break;

	case 'a':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_DOUBLE;
		break;

	case 'A':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_DOUBLE;
		break;

	case 'c':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_CHAR;
		break;

	case 'E':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_DOUBLE;
		break;

	case 'e':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_DOUBLE;
		break;

	case 'F':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_DOUBLE;
		break;

	case 'f':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_DOUBLE;
		break;

	case 'x':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_INT;
		break;

	case 'X':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_INT;
		break;

	case 'u':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_INT;
		break;

	case 'o':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_INT;
		break;

	case 'd':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_INT;
		break;

	case 'i':
		ctx->printf_args_types[ctx->printf_args_cnt] = PA_INT;
		break;

	default:
		ctx->printf_args_types[ctx->printf_args_cnt] = 0;
		/*
		 * Better not to challenge the printf here
		 * log_err("unknown varargs format '%d'", __info->spec);
		 */
	}

	/* setup flags */
	if (ctx->printf_args_types[ctx->printf_args_cnt]) {
		if (__info->is_long_double) {
			ctx->printf_args_types[ctx->printf_args_cnt] |=
				PA_FLAG_LONG_LONG;
		} else if (__info->is_short) {
			ctx->printf_args_types[ctx->printf_args_cnt] |=
				PA_FLAG_SHORT;
		} else if (__info->is_long) {
			ctx->printf_args_types[ctx->printf_args_cnt] |=
				PA_FLAG_LONG;
		} else if (__info->is_quad) {
			ctx->printf_args_types[ctx->printf_args_cnt] |=
				PA_FLAG_QUAD;
		} else if (__info->is_intmax) {
			ctx->printf_args_types[ctx->printf_args_cnt] |=
				PA_FLAG_INTMAX;
		} else if (__info->is_ptrdiff) {
			ctx->printf_args_types[ctx->printf_args_cnt] |=
				PA_FLAG_PTRDIFF;
		} else if (__info->is_size) {
			ctx->printf_args_types[ctx->printf_args_cnt] |=
				PA_FLAG_SIZE;
		}

	}

	ctx->printf_args_cnt++;

	return 0;
}

static int
	as_render(FILE *f, const struct printf_info *pi, const void *const *hz)
{
	return 0;
}

static inline void as_tear_printf_context(void)
{
	struct AsThreadContext *thread_ctx;

	thread_ctx = (struct AsThreadContext *)
		retrace_real_impls.pthread_getspecific(as_thread_ctx_key);

	retrace_real_impls.free(thread_ctx);
	retrace_real_impls.pthread_setspecific(
		as_thread_ctx_key, NULL);

	/* the only way to stop xprintf once registered? */
	__use_xprintf = 0;
}

static inline struct AsThreadContext *as_setup_printf_context(void)
{
	struct AsThreadContext *thread_ctx;

	thread_ctx = (struct AsThreadContext *)
		retrace_real_impls.pthread_getspecific(as_thread_ctx_key);

	/* context should be destroyed after each invocation
	 * by as_tear_printf_context
	 */
	if (thread_ctx == NULL) {

		thread_ctx = (struct AsThreadContext *)
			retrace_real_impls.malloc(sizeof(struct AsThreadContext));

		if (thread_ctx == NULL) {
			log_err("failed to malloc thread_ctx");
			return NULL;
		}

		if (retrace_real_impls.pthread_setspecific(
			as_thread_ctx_key, thread_ctx)) {

			log_err("failed to set specific thread key");
			retrace_real_impls.free(thread_ctx);

			return NULL;
		}

		/* reset context data */
		retrace_real_impls.memset(thread_ctx, 0, sizeof(*thread_ctx));

		/* register every possible format
		 * a little brutal, but it will ensure that no parameter is missed
		 */
		for (char j = 'a'; j <= 'z'; j++)
			register_printf_function(j,
					as_render,
					as_arginfo_function);

		for (char i = 'A'; i <= 'Z'; i++)
			register_printf_function(i,
				as_render,
				as_arginfo_function);

	} else {
		log_err("printf ctx already exist");
		return NULL;
	}

	return thread_ctx;
}

long retrace_as_call_real(const void *real_impl,
	const struct FuncParam params[],
	int params_cnt)
{
	long ret_val;
	long *vals;
	int i;
	unsigned long params_cnt_uli;

	/* prep param values to keep the asm part clean */
	params_cnt_uli = params_cnt;
	vals = (long *) retrace_real_impls.malloc(
			sizeof(long) * params_cnt);

	for (i = 0; i != params_cnt; i++)
		vals[i] = params[i].val;

	asm volatile (
				/* save regs that we gonna use,
				 * dont rely on clobbed regs
				 */
				"pushq %%rax;"
				"pushq %%r10;"
				"pushq %%r11;"

				"pushq %%r12;"
				"pushq %%r13;"
				"pushq %%r14;"
				"pushq %%r15;"

				"pushq %%rdi;"
				"pushq %%rsi;"
				"pushq %%rdx;"
				"pushq %%rcx;"
				"pushq %%r8;"
				"pushq %%r9;"
				"pushq %%rbx;"

				/* save params to registers,
				 * we might loose rbp to params
				 */
				"movq %3, %%r12;"
				"movq %2, %%r13;"
				"movq %1, %%r14;"
				"leaq %0, %%r15;"

				/* load param count */
				"movq %%r12, %%r10;"

				/* align rsp,
				 * store in rbx status of the alignment.
				 * be careful - number of previous pushes (14)
				 * is taken into account
				 */
				"xorq %%rbx, %%rbx;"
				"subq $6, %%r10;"
				"ja _call_ret_ParamsOnStack;"
				/* set params count on stack to 0 */
				"xorq %%r10, %%r10;"

				"_call_ret_ParamsOnStack:;"
				"andq $1, %%r10;"
				"jz _call_ret_StackAligned;"
				"subq $8, %%rsp;"

				/* mark stack unaligned */
				"movq $1, %%rbx;"

				"_call_ret_StackAligned:;"
				/* load param count again,
				 * now for parameter passing
				 */
				"movq %%r12, %%r10;"

				/* no params ? */
				"cmpq $0, %%r10;"
				"jz _call_ret_ParamSetupDone;"

				/* load first param void**,
				 * use rax for redirect
				 */
				//"movq (%%r13), %%rax;"
				//"movq (%%rax), %%rdi;"
				"movq (%%r13), %%rdi;"

				/* more params? */
				"decq %%r10;"
				"jz _call_ret_ParamSetupDone;"

				/* load second param */
				//"movq 8(%%r13), %%rax;"
				//"movq (%%rax), %%rsi;"
				"movq 8(%%r13), %%rsi;"

				/* more params? */
				"decq %%r10;"
				"jz _call_ret_ParamSetupDone;"

				/* load third param */
				//"movq 16(%%r13), %%rax;"
				//"movq (%%rax), %%rdx;"
				"movq 16(%%r13), %%rdx;"

				/* more params? */
				"decq %%r10;"
				"jz _call_ret_ParamSetupDone;"

				/* load forth param */
				//"movq 24(%%r13), %%rax;"
				//"movq (%%rax), %%rcx;"
				"movq 24(%%r13), %%rcx;"

				/* more params? */
				"decq %%r10;"
				"jz _call_ret_ParamSetupDone;"

				/* load fifth param */
				//"movq 32(%%r13), %%rax;"
				//"movq (%%rax), %%r8;"
				"movq 32(%%r13), %%r8;"

				/* more params? */
				"decq %%r10;"
				"jz _call_ret_ParamSetupDone;"

				/* load sixth param */
				//"movq 40(%%r13), %%rax;"
				//"movq (%%rax), %%r9;"
				"movq 40(%%r13), %%r9;"

				/* more params? */
				"decq %%r10;"
				"jz _call_ret_ParamSetupDone;"

				/* push the rest params on stack, use r11*/
				"movq %%r13, %%r11;"
				"addq $48, %%r11;"

				"_call_ret_MoreParams:;"
				"movq (%%r11), %%rax;"
				//"pushq (%%rax);"
				"pushq %%rax;"

				"decq %%r10;"
				"jz _call_ret_ParamSetupDone;"

				"addq $8, %%r11;"
				"jmp _call_ret_MoreParams;"

				"_call_ret_ParamSetupDone:;"
				/* TODO: support float params */
				"xorq %%rax, %%rax;"
				"callq *%%r14;"

				/* save return value in ret_val */
				"movq %%rax, (%%r15);"

				/* is stack aligned? */
				"cmpq $1, %%rbx;"
				"jne _call_ret_Restore;"
				"addq $8, %%rsp;"

				"_call_ret_Restore:;"
				/* pop params from stack if any */
				"movq %%r12, %%r10;"
				"subq $6, %%r10;"
				"jb _call_ret_NoParamsOnStack;"
				"movb $3, %%cl;"
				"shlq %%cl, %%r10;"
				"addq %%r10, %%rsp;"

				"_call_ret_NoParamsOnStack:;"

				"popq %%rbx;"
				"popq %%r9;"
				"popq %%r8;"
				"popq %%rcx;"
				"popq %%rdx;"
				"popq %%rsi;"
				"popq %%rdi;"

				"popq %%r15;"
				"popq %%r14;"
				"popq %%r13;"
				"popq %%r12;"
				"popq %%r11;"
				"popq %%r10;"
				"popq %%rax;"

				: "=m"(ret_val)
				: "m"(real_impl), "m"(vals), "m"(params_cnt_uli)
				: "memory");

	retrace_real_impls.free(vals);
	return ret_val;
}

void retrace_as_abort(void *arch_spec_ctx, long ret_val)
{
	struct WrapperSystemVFrame *wrapper_frame_top;

	wrapper_frame_top = arch_spec_ctx;
	wrapper_frame_top->call_real_flag = 0;
	wrapper_frame_top->ret_val = ret_val;
}

void retrace_as_sched_real(void *arch_spec_ctx, void *real_impl)
{
	struct WrapperSystemVFrame *wrapper_frame_top;

	wrapper_frame_top = arch_spec_ctx;
	wrapper_frame_top->call_real_flag = 1;
	wrapper_frame_top->real_impl = real_impl;
}

int retrace_as_setup_params(
	void *arch_spec_ctx,
	const struct FuncPrototype *proto,
	struct FuncParam params[],
	int *params_cnt)
{
	int i;
	int printf_params;
	int param_idx;
	int params_on_stack;
	struct WrapperSystemVFrame *wrapper_frame_top;
	const struct DataType *dt;
	struct AsThreadContext *as_ctx;
	int use_unk_dt;

	/* check whether there is enough space for prototyped params */
	if (*params_cnt < proto->params_cnt) {
		log_err("too many prototyped params for '%s', no space for %d more",
			proto->name,
			(*params_cnt - proto->params_cnt) * -1);
		return 0;
	}

	/* set up prototyped params */
	wrapper_frame_top = (struct WrapperSystemVFrame *) arch_spec_ctx;
	params_on_stack = proto->params_cnt - 6;

	for (param_idx = 0; param_idx != proto->params_cnt; param_idx++) {

		/* reset param */
		retrace_real_impls.memset(&params[param_idx].param_meta,
				0,
				sizeof(struct ParamMeta));

		/* setup meta */
		retrace_real_impls.memcpy(&params[param_idx].param_meta,
				&proto->params[param_idx],
				sizeof(struct ParamMeta));

		/* setup datatype */
		params[param_idx].data_type =
			retrace_datatype_get(proto->params[param_idx].type_name);

		/* setup value */
		if (param_idx < 6) {
			/* get param from reg */
			switch (param_idx) {
			case 0:
				params[param_idx].val =
					wrapper_frame_top->real_rdi;
				break;
			case 1:
				params[param_idx].val =
					wrapper_frame_top->real_rsi;
				break;
			case 2:
				params[param_idx].val =
					wrapper_frame_top->real_rdx;
				break;
			case 3:
				params[param_idx].val =
					wrapper_frame_top->real_rcx;
				break;
			case 4:
				params[param_idx].val =
					wrapper_frame_top->real_r8;
				break;
			case 5:
				params[param_idx].val =
					wrapper_frame_top->real_r9;
				break;
			}
		} else {
			/* get param from stack */

			/* assume sizeof(void*) == sizeof(long) */
			params[param_idx].val =
				(wrapper_frame_top->real_rsp +
					sizeof(void *) * (params_on_stack - param_idx));
		}
	}

	/* set up varargs params */
	if (proto->fmt == FAT_NOVARARGS) {
		/* we`re done */
		*params_cnt = proto->params_cnt;
		return 1;
	}

	/* check whether format is supported */
	if ((proto->fmt != FAT_NOVARARGS) &&
		(proto->fmt != FAT_PRINTF)) {

		log_err("varargs format '%d' is not supported for func '%s'",
			proto->fmt, proto->name);
		return 0;
	}

	/* calculate the expected number of params - disabled
	 * Do not use as_calc_params_specifiers since it is not perfect,
	 * it is better to rely on provided parsing facilities
	 */
#ifdef RETRACE_V2_USE_CALC_PARAMS
	printf_params =
		as_calc_params_specifiers(
			(const char *) params[proto->fmt_param_idx].val);
	if (!printf_params) {
		*params_cnt = proto->params_cnt;
		return 1;
	}
#endif

	/* setup varargs params, it is an ugly
	 * implementation due to BSD implementation of printf.h
	 */

	retrace_real_impls.pthread_mutex_lock(&as_printf_mtx);

	as_ctx = as_setup_printf_context();
	if (as_ctx == NULL) {
		log_err("failed to get as_ctx for func '%s'", proto->name);
		retrace_real_impls.pthread_mutex_unlock(&as_printf_mtx);
		return 0;
	}

	as_ctx->printf_args_cnt = 0;
	/* callbacks will setup param types */

	snprintf(NULL, 0, (const char *) params[proto->fmt_param_idx].val);

	as_tear_printf_context();
	retrace_real_impls.pthread_mutex_unlock(&as_printf_mtx);

#ifdef RETRACE_V2_USE_CALC_PARAMS
	if (printf_params != as_ctx->printf_args_cnt) {
		log_warn("problem parsing params format string for func '%s'",
			proto->name);

		/* set up printf_params as unknown so we won't crash */
		use_unk_dt = 1;
	} else
		use_unk_dt = 0;
#else
	printf_params = as_ctx->printf_args_cnt;
	use_unk_dt = 0;
#endif

	for (i = 0; i != printf_params; i++, param_idx++) {
		/* prep param meta */
		if (use_unk_dt)
			dt = retrace_datatype_get_unk_dt();
		else
			dt = retrace_datatype_printf_to_dt(as_ctx->printf_args_types[i]);

		/* reset param */
		retrace_real_impls.memset(&params[param_idx].param_meta,
			0,
			sizeof(struct ParamMeta));

		/* setup meta */
		retrace_real_impls.snprintf(params[param_idx].param_meta.name,
			sizeof(params[param_idx].param_meta.name),
			"vararg%02d",
			i);

		retrace_real_impls.strcpy(params[param_idx].param_meta.type_name,
			dt->name);

		params[param_idx].param_meta.modifiers = CDM_NOMOD;
		/* FIXME: How to know the direction of the param? */
		params[param_idx].param_meta.direction = PDIR_IN;

		/* fixup pointer type to string*/
		if ((as_ctx->printf_args_types[i] & ~PA_FLAG_MASK) == PA_STRING) {
			params[param_idx].param_meta.modifiers |= CDM_POINTER;
			retrace_real_impls.strcpy(
				params[param_idx].param_meta.ref_type_name,
				"sz");
		}

		/* setup datatype */
		params[param_idx].data_type = dt;

		/* setup value */
		if (param_idx < 6) {
			/* get param from reg */
			switch (param_idx) {
			case 0:
				params[param_idx].val =
					wrapper_frame_top->real_rdi;
				break;
			case 1:
				params[param_idx].val =
					wrapper_frame_top->real_rsi;
				break;
			case 2:
				params[param_idx].val =
					wrapper_frame_top->real_rdx;
				break;
			case 3:
				params[param_idx].val =
					wrapper_frame_top->real_rcx;
				break;
			case 4:
				params[param_idx].val =
					wrapper_frame_top->real_r8;
				break;
			case 5:
				params[param_idx].val =
					wrapper_frame_top->real_r9;
				break;
			}
		} else {
			/* get param from stack */

			/* assume sizeof(void*) == sizeof(long) */
			params[param_idx].val =
				(wrapper_frame_top->real_rsp +
					sizeof(void *) * (params_on_stack - param_idx));
		}
	}

	*params_cnt = param_idx;
	return 1;
}

void retrace_as_intercept_done(void *arch_spec_ctx,
	long ret_val)
{
	((struct WrapperSystemVFrame *) arch_spec_ctx)->ret_val = ret_val;

	((struct WrapperSystemVFrame *) arch_spec_ctx)->call_real_flag = 0;
}

void retrace_as_cancel_sched_real(void *arch_spec_ctx)
{
	((struct WrapperSystemVFrame *) arch_spec_ctx)->call_real_flag = 0;
}

void retrace_as_set_ret_val(void *arch_spec_ctx,
	long ret_val)
{
	((struct WrapperSystemVFrame *) arch_spec_ctx)->ret_val = ret_val;
}

int retrace_as_init(void)
{
	return 0;
}

int retrace_as_init_late(void)
{
	int key_res;

	key_res = retrace_real_impls.pthread_key_create(&as_thread_ctx_key,
		as_thread_ctx_destructor);

	if (key_res) {
		log_err("failed to create pthread_key");
		return key_res;
	}

	if (retrace_real_impls.pthread_mutex_init(&as_printf_mtx, NULL)) {
		log_err("failed to create pthread_mutex");
		retrace_real_impls.pthread_key_delete(as_thread_ctx_key);
		return 1;
	}

	return key_res;
}

void *retrace_as_get_real_safe(const char *real_impl)
{
	/* could not find any other suitable method */
	return dlsym(RTLD_NEXT, real_impl);
}
