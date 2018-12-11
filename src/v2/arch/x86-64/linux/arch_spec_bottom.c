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

#include "engine.h"
#include "real_impls.h"
#include "logger.h"
#include "printf.h"

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
				"movq %%rax, (%%r15);"

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
	int *types;

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

	printf_params =
		parse_printf_format(
			(const char *) params[proto->fmt_param_idx].val,
			0,
			NULL);

	if (!printf_params) {
		/* we`re done */
		*params_cnt = proto->params_cnt;
		return 1;
	}

	types = (int *)
		retrace_real_impls.malloc(sizeof(int) * printf_params);

	printf_params =
		parse_printf_format(
			(const char *) params[proto->fmt_param_idx].val,
			printf_params,
			types);

	for (i = 0; i != printf_params; i++, param_idx++) {
		/* prep param meta */
		dt = retrace_datatype_printf_to_dt(types[i]);

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
		if ((types[i] & ~PA_FLAG_MASK) == PA_STRING) {
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
	return 0;
}

extern void *_dl_sym(void *handle, const char *symbol, const void *rtraddr);

void *retrace_as_get_real_safe(const char *real_impl)
{
	return _dl_sym(RTLD_NEXT, real_impl, __func__);
}

