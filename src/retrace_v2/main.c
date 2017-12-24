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
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <stdlib.h>

#include "real_impls.h"
#include "engine.h"

#if 0

static inline int call_orig_x86_32_cdecl_ret(
	void *orig_impl,
	void *params_start,
	unsigned int params_cnt)
{
	int ret_val;

	asm volatile (
				"pushl %%edx;"
				"pushl %%ecx;"
				"pushl %%ebx;"

				/* Have no idea why, but sometimes
				 * callee trashes the ebp...
				 */
				"movl %%ebp, %%ebx;"

				"movl %3, %%ecx;"
				"cmpl $0, %%ecx;"
				"je _call_orig_x86_32_cdecl_ret_ParamSetupDone;"
				"movl %2, %%eax;"
				"_call_orig_x86_32_cdecl_ret_StoreParam:;"
				"movl (%%eax), %%edx;"
				"pushl %%edx;"
				"addl $4, %%eax;"
				"loop _call_orig_x86_32_cdecl_ret_StoreParam;"
				"_call_orig_x86_32_cdecl_ret_ParamSetupDone:;"
				"call *%1;"

				/* restore ebp, before addressing the locals */
				"movl %%ebx, %%ebp;"
				"movl %%eax, %0;"
				"movl %3, %%eax;"
				"movb $2, %%cl;"
				"shll %%cl, %%eax;"
				"addl %%eax, %%esp;"
				"popl %%ebx;"
				"popl %%ecx;"
				"popl %%edx;"
				: "=m"(ret_val)
				: "m"(orig_impl),
				  "m"(params_start),
				  "m"(params_cnt)
				: "memory", "%eax");

	return ret_val;
}

static inline void call_orig_x86_32_cdecl(
	void *orig_impl,
	void *params_start,
	unsigned int params_cnt)
{
	asm volatile (
				"pushl %%edx;"
				"pushl %%ecx;"
				"pushl %%ebx;"
				/* Have no idea why,
				 * but sometimes callee trashes the ebp...
				 */
				"movl %%ebp, %%ebx;"

				"movl %2, %%ecx;"
				"cmpl $0, %%ecx;"
				"je _call_orig_x86_32_cdecl_ParamSetupDone;"
				"movl %1, %%eax;"
				"_call_orig_x86_32_cdecl_StoreParam:;"
				"movl (%%eax), %%edx;"
				"pushl %%edx;"
				"addl $4, %%eax;"
				"loop _call_orig_x86_32_cdecl_StoreParam;"
				"_call_orig_x86_32_cdecl_ParamSetupDone:;"
				"call *%0;"

				/* restore ebp, before addressing the locals */
				"movl %%ebx, %%ebp;"
				"movl %2, %%eax;"
				"movb $2, %%cl;"
				"shll %%cl, %%eax;"
				"addl %%eax, %%esp;"
				"popl %%ebx;"
				"popl %%ecx;"
				"popl %%edx;"
				:
				: "m"(orig_impl),
				  "m"(params_start),
				  "m"(params_cnt)
				: "memory", "%eax");
}

#endif

int retrace_inited;

__attribute__((constructor(101)))
static void retrace_main(void)
{
	if (retrace_real_impls_init_safe())
		return;

	if (retrace_engine_init())
		return;

	retrace_inited = 1;
}

#ifndef RETRACE_SIMULATE_INTERCEPT
/* Simulation of LD_PRELOAD substitution */

/*
 * char *getenv_wrapper(char *env_name);
 * char *write_wrapper(int fd, const void *buf, size_t count);
 * size_t writev_wrapper(int fd, const struct iovec *iov, int iovcnt);
 */

#else

#define getenv_wrapper(env_name) getenv(env_name)
#define write_wrapper(fd, buf, count) write(fd, buf, count)
#define writev_wrapper(fd, iov, iovcnt) writev(fd, iov, iovcnt)

#endif
