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

#include <printf.h>
#include <pthread.h>

#include "engine.h"
#include "real_impls.h"
#include "logger.h"

#define log_err(fmt, ...) \
	retrace_logger_log(ARCH, ERROR, fmt, ##__VA_ARGS__)

#define log_info(fmt, ...) \
	retrace_logger_log(ARCH, INFO, fmt, ##__VA_ARGS__)

#define log_warn(fmt, ...) \
	retrace_logger_log(ARCH, WARN, fmt, ##__VA_ARGS__)

#define log_dbg(fmt, ...) \
	retrace_logger_log(ARCH, DEBUG, fmt, ##__VA_ARGS__)

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

struct RealImpls {
	char* (*getsectdata)(
	   const char *segname,
	   const char *sectname,
	   unsigned long *size);
};

static struct RealImpls retrace_as_real_impls;

struct AsThreadContext {
	printf_domain_t domain;

	/*
	 * this is to emulate linux printf mechanics, 0 means unknown type
	 * values are PA_* or 0 for unknown
	 */
	int printf_args_types[ENGINE_MAXCOUNT_PARAMS];
	int printf_args_cnt;
};
static pthread_key_t as_thread_ctx_key;

static void as_thread_ctx_destructor(void *thread_ctx)
{
	free_printf_domain(((struct AsThreadContext *) thread_ctx)->domain);
	retrace_real_impls.free(thread_ctx);
	retrace_real_impls.pthread_key_delete(as_thread_ctx_key);
}

static int as_arginfo_function(const struct printf_info *__info,
	     size_t __n, int *__argtypes)
{
	struct AsThreadContext *ctx =
		(struct AsThreadContext *) __info->context;

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
	as_render(FILE *stream, const struct printf_info *info,
		const void *const *args)
{
	return 0;
}

static inline struct AsThreadContext *as_get_thread_context(void)
{
	struct AsThreadContext *thread_ctx;

	thread_ctx = (struct AsThreadContext *)
		retrace_real_impls.pthread_getspecific(as_thread_ctx_key);

	/* try to create if does not exist */
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

		thread_ctx->domain = new_printf_domain();
		if (thread_ctx->domain == NULL) {
			log_err("failed to create printf domain");

			retrace_real_impls.pthread_key_delete(as_thread_ctx_key);
			retrace_real_impls.free(thread_ctx);
			return NULL;
		}

		/* register every possible format
		 * a little brutal, but it will ensure that no parameter is missed
		 */
		for (char j = 'a'; j <= 'z'; j++)
			register_printf_domain_function(thread_ctx->domain,
				j,
				as_render,
				as_arginfo_function, (void *) thread_ctx);

		for (char i = 'A'; i <= 'Z'; i++)
			register_printf_domain_function(thread_ctx->domain,
				i,
				as_render,
				as_arginfo_function, (void *) thread_ctx);
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
				"ja L_call_ret_ParamsOnStack;"
				/* set params count on stack to 0 */
				"xorq %%r10, %%r10;"

				"L_call_ret_ParamsOnStack:;"
				"andq $1, %%r10;"
				"jz L_call_ret_StackAligned;"
				"subq $8, %%rsp;"

				/* mark stack unaligned */
				"movq $1, %%rbx;"

				"L_call_ret_StackAligned:;"
				/* load param count again,
				 * now for parameter passing
				 */
				"movq %%r12, %%r10;"

				/* no params ? */
				"cmpq $0, %%r10;"
				"jz L_call_ret_ParamSetupDone;"

				/* load first param void**,
				 * use rax for redirect
				 */
				//"movq (%%r13), %%rax;"
				//"movq (%%rax), %%rdi;"
				"movq (%%r13), %%rdi;"

				/* more params? */
				"decq %%r10;"
				"jz L_call_ret_ParamSetupDone;"

				/* load second param */
				//"movq 8(%%r13), %%rax;"
				//"movq (%%rax), %%rsi;"
				"movq 8(%%r13), %%rsi;"

				/* more params? */
				"decq %%r10;"
				"jz L_call_ret_ParamSetupDone;"

				/* load third param */
				//"movq 16(%%r13), %%rax;"
				//"movq (%%rax), %%rdx;"
				"movq 16(%%r13), %%rdx;"

				/* more params? */
				"decq %%r10;"
				"jz L_call_ret_ParamSetupDone;"

				/* load forth param */
				//"movq 24(%%r13), %%rax;"
				//"movq (%%rax), %%rcx;"
				"movq 24(%%r13), %%rcx;"

				/* more params? */
				"decq %%r10;"
				"jz L_call_ret_ParamSetupDone;"

				/* load fifth param */
				//"movq 32(%%r13), %%rax;"
				//"movq (%%rax), %%r8;"
				"movq 32(%%r13), %%r8;"

				/* more params? */
				"decq %%r10;"
				"jz L_call_ret_ParamSetupDone;"

				/* load sixth param */
				//"movq 40(%%r13), %%rax;"
				//"movq (%%rax), %%r9;"
				"movq 40(%%r13), %%r9;"

				/* more params? */
				"decq %%r10;"
				"jz L_call_ret_ParamSetupDone;"

				/* push the rest params on stack, use r11*/
				"movq %%r13, %%r11;"
				"addq $48, %%r11;"

				"L_call_ret_MoreParams:;"
				"movq (%%r11), %%rax;"
				//"pushq (%%rax);"
				"pushq %%rax;"

				"decq %%r10;"
				"jz L_call_ret_ParamSetupDone;"

				"addq $8, %%r11;"
				"jmp L_call_ret_MoreParams;"

				"L_call_ret_ParamSetupDone:;"
				/* TODO: support float params */
				"xorq %%rax, %%rax;"
				"callq *%%r14;"

				/* save return value in ret_val */
				"movq %%rax, (%%r15);"

				/* is stack aligned? */
				"cmpq $1, %%rbx;"
				"jne L_call_ret_Restore;"
				"addq $8, %%rsp;"

				"L_call_ret_Restore:;"
				/* pop params from stack if any */
				"movq %%r12, %%r10;"
				"subq $6, %%r10;"
				"jb L_call_ret_NoParamsOnStack;"
				"movb $3, %%cl;"
				"shlq %%cl, %%r10;"
				"addq %%r10, %%rsp;"

				"L_call_ret_NoParamsOnStack:;"

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
	int i, j;
	int params_on_stack;
	struct WrapperSystemVFrame *wrapper_frame_top;
	int cnt_left;
	const struct DataType *dt;
	struct AsThreadContext *as_ctx;
	printf_comp_t pc;

	cnt_left = *params_cnt;
	if (cnt_left > proto->params_cnt)
		cnt_left = proto->params_cnt;

	/* set up prototyped params */
	wrapper_frame_top = (struct WrapperSystemVFrame *) arch_spec_ctx;
	params_on_stack = proto->params_cnt - 6;

	for (i = 0; i != cnt_left; i++) {

		/* reset param */
		retrace_real_impls.memset(&params[i].param_meta,
				0,
				sizeof(struct ParamMeta));

		/* setup meta */
		retrace_real_impls.memcpy(&params[i].param_meta,
				&proto->params[i],
				sizeof(struct ParamMeta));

		/* setup datatype */
		params[i].data_type = retrace_datatype_get(proto->params[i].type_name);

		/* setup value */
		if (i < 6) {
			/* get param from reg */
			switch (i) {
			case 0:
				params[i].val =
					wrapper_frame_top->real_rdi;
				break;
			case 1:
				params[i].val =
					wrapper_frame_top->real_rsi;
				break;
			case 2:
				params[i].val =
					wrapper_frame_top->real_rdx;
				break;
			case 3:
				params[i].val =
					wrapper_frame_top->real_rcx;
				break;
			case 4:
				params[i].val =
					wrapper_frame_top->real_r8;
				break;
			case 5:
				params[i].val =
					wrapper_frame_top->real_r9;
				break;
			}
		} else {
			/* get param from stack */

			/* assume sizeof(void*) == sizeof(long) */
			params[i].val =
				(wrapper_frame_top->real_rsp +
					sizeof(void *) * (params_on_stack - i));
		}
	}

	/* save count of set up params before parsing varargs */


	/* if no varargs then we are done */
	if (proto->fmt == FAT_NOVARARGS) {
		*params_cnt = i;
		return 0;
	}

	/* if format is not printf, return as no supported */
	if (proto->fmt != FAT_PRINTF) {
		log_err("varargs format '%d' is not supported for func '%s'",
			proto->fmt, proto->name);
		*params_cnt = i;
		return 0;
	}

	/* if no single % then we are done */
	for (j = 0;
		*(((char *) params[proto->fmt_param_idx].val) + j);
		j++) {

		if (
			(*(((char *)params[proto->fmt_param_idx].val) + j) == '%') &&
			(j > 0) &&
			(*(((char *)params[proto->fmt_param_idx].val) + j - 1) != '%') &&
			(*(((char *)params[proto->fmt_param_idx].val) + j + 1) != '%')
		)
			break;
	}

	if (*(((char *) params[proto->fmt_param_idx].val) + j) == 0) {
		/* no format specifier */
		*params_cnt = i;
		return 0;
	}

	/* space exhausted?, we will probably crash */
	cnt_left = *params_cnt - proto->params_cnt;

	if (cnt_left <= 0) {
		log_warn("too many varargs params for '%s', no space for %d more",
			proto->name, cnt_left * -1);
		return cnt_left;
	}

	/* setup varargs params */
	as_ctx = as_get_thread_context();
	if (as_ctx == NULL) {
		log_err("failed to get as_ctx for func '%s'", proto->name);
		return 0;
	}

	as_ctx->printf_args_cnt = 0;
	/* callbacks will setup param types */
	pc = new_printf_comp(as_ctx->domain,
		NULL,
		(const char *) params[proto->fmt_param_idx].val);
	if (pc == NULL) {
		log_err("new_printf_comp failed for func '%s'", proto->name);
		return 0;
	}
	free_printf_comp(pc);

	if (!as_ctx->printf_args_cnt) {
		log_err("parse_printf_format failed to parse param idx '%d'"
				"for func '%s'", proto->fmt_param_idx, proto->name);
		return 0;
	}

	if (cnt_left > as_ctx->printf_args_cnt)
		cnt_left = as_ctx->printf_args_cnt;

	for (j = 0; j != cnt_left; j++) {
		/* prep param meta */
		dt = retrace_datatype_printf_to_dt(as_ctx->printf_args_types[j]);
		if (dt == NULL) {
			log_warn("argtype '%d' is not supported for func '%s', skipping",
				as_ctx->printf_args_types[j], proto->name);

			/* TODO Add unknown since it will not be passed on stack */
			continue;
		}

		/* reset param */
		retrace_real_impls.memset(&params[i].param_meta,
			0,
			sizeof(struct ParamMeta));

		/* setup meta */
		retrace_real_impls.snprintf(params[i].param_meta.name,
			sizeof(params[i].param_meta.name),
			"vararg%02d",
			j);

		retrace_real_impls.strcpy(params[i].param_meta.type_name, dt->name);
		params[i].param_meta.modifiers = CDM_NOMOD;
		/* FIXME: How to know the direction of the param? */
		params[i].param_meta.direction = PDIR_IN;

		/* fixup pointer type to string*/
		if ((as_ctx->printf_args_types[j] & ~PA_FLAG_MASK) == PA_STRING) {
			params[i].param_meta.modifiers |= CDM_POINTER;
			retrace_real_impls.strcpy(params[i].param_meta.ref_type_name, "sz");
		}

		/* setup datatype */
		params[i].data_type = dt;

		/* setup value */
		if (i < 6) {
			/* get param from reg */
			switch (i) {
			case 0:
				params[i].val =
					wrapper_frame_top->real_rdi;
				break;
			case 1:
				params[i].val =
					wrapper_frame_top->real_rsi;
				break;
			case 2:
				params[i].val =
					wrapper_frame_top->real_rdx;
				break;
			case 3:
				params[i].val =
					wrapper_frame_top->real_rcx;
				break;
			case 4:
				params[i].val =
					wrapper_frame_top->real_r8;
				break;
			case 5:
				params[i].val =
					wrapper_frame_top->real_r9;
				break;
			}
		} else {
			/* get param from stack */

			/* assume sizeof(void*) == sizeof(long) */
			params[i].val =
				(wrapper_frame_top->real_rsp +
					sizeof(void *) * (params_on_stack - i));
		}

		/* advance next param */
		i++;
	}

	/* save count of set up params before parsing varargs */
	*params_cnt = i;
	return as_ctx->printf_args_cnt - cnt_left;
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

	if (key_res)
		log_err("failed to create pthread_key");

	return key_res;
}

void *retrace_as_get_real_safe(const char *real_impl)
{
	unsigned long size;
	int i;

	/* This has to be aligned with __retrace_rimpls */
	struct __retrace_rimpls {
		const char func_name[MAXLEN_FUNC_NAME];
		void *real_impl;
	} *p, *eos;

	retrace_as_get_section_info("__DATA", "__retrace_rimpls", &p, &size);

	//log_info("Seraching for real impl for '%s'", real_impl);

	eos = (struct __retrace_rimpls *) (((char *) p) + size);
	while (p && p != eos) {
		i = 0;
		while (p->func_name[i] &&
			p->func_name[i] == real_impl[i]) {
			i++;
		}

		if (p->func_name[i] == real_impl[i])
			return p->real_impl;

		p++;
	}

	return 0;
}
