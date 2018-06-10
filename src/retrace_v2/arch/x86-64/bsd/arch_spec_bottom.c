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

long int retrace_as_call_real(const void *real_impl,
	const struct ParamMeta *params_meta,
	long int params[])
{
	long int ret_val;
	unsigned long int params_cnt;
	const struct ParamMeta *p;

	/* calc params count
	 * TODO: Improve speed (calc once)
	 */
	params_cnt = 0;
	p = params_meta;
	while (retrace_real_impls.strlen(p->name)) {
		params_cnt++;
		p++;
	}

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
				: "m"(real_impl), "m"(params), "m"(params_cnt)
				: "memory");

	return ret_val;
}

void retrace_as_abort(void *arch_spec_ctx, long int ret_val)
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

void retrace_as_setup_params(
	void *arch_spec_ctx,
	const struct ParamMeta *params_meta,
	long int params[])
{
	int i;
	int params_on_stack;
	int params_cnt;
	struct WrapperSystemVFrame *wrapper_frame_top;

	/* calc params count
	 * TODO: Improve speed (calc once)
	 */
	params_cnt = 0;
	while (retrace_real_impls.strlen(
		params_meta[params_cnt].name)) {
		params_cnt++;
	}

	wrapper_frame_top = (struct WrapperSystemVFrame *) arch_spec_ctx;
	for (i = 0; i != params_cnt; i++) {

		/* setup data*/
		if (i < 6) {
			/* get param from reg */
			switch (i) {
			case 0:
				params[i] =
					wrapper_frame_top->real_rdi;
				break;
			case 1:
				params[i] =
					wrapper_frame_top->real_rsi;
				break;
			case 2:
				params[i] =
					wrapper_frame_top->real_rdx;
				break;
			case 3:
				params[i] =
					wrapper_frame_top->real_rcx;
				break;
			case 4:
				params[i] =
					wrapper_frame_top->real_r8;
				break;
			case 5:
				params[i] =
					wrapper_frame_top->real_r9;
				break;
			}
		} else {
			/* get param from stack */
			params_on_stack = params_cnt - 6;

			/* assume sizeof(void*) == sizeof(long int) */
			params[i] =
				(wrapper_frame_top->real_rsp +
					sizeof(void *) * (params_on_stack - i));
		}
	}
}

void retrace_as_intercept_done(void *arch_spec_ctx,
	long int ret_val)
{
	((struct WrapperSystemVFrame *) arch_spec_ctx)->ret_val = ret_val;

	((struct WrapperSystemVFrame *) arch_spec_ctx)->call_real_flag = 0;
}

void retrace_as_cancel_sched_real(void *arch_spec_ctx)
{
	((struct WrapperSystemVFrame *) arch_spec_ctx)->call_real_flag = 0;
}

void retrace_as_set_ret_val(void *arch_spec_ctx,
	long int ret_val)
{
	((struct WrapperSystemVFrame *) arch_spec_ctx)->ret_val = ret_val;
}

int retrace_as_init(void)
{
	return 0;
}

void *retrace_as_get_real_safe(const char *real_impl)
{
	/* could not find any other suitable method */
	return dlsym(RTLD_NEXT, real_impl);
}

