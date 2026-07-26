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

/*
 * Register translation. The trace loop hands us a raw regset blob
 * obtained from PTRACE_GETREGSET; we decode it into the portable
 * retrace_ptrace_frame. Symmetric write path lets the engine modify
 * args (or skip the syscall entirely by setting the return value).
 *
 * Two architectures are handled here: x86_64 (struct user_regs_struct)
 * and aarch64 (struct user_pt_regs). On any other host the detect
 * function returns UNKNOWN and the loop bails.
 */

#include "translate.h"

#include <string.h>
#include <stdint.h>

/* Register layouts are only available on Linux. Apple Silicon defines
 * __aarch64__ but has no <asm/ptrace.h>; macOS x86_64 defines neither
 * __x86_64__ nor the Linux <sys/user.h>. Gate each arch block on BOTH
 * Linux and the architecture so the file compiles cleanly everywhere
 * (the non-Linux build becomes a no-op stub).
 */
#if defined(__linux__) && defined(__x86_64__)
#define RETRACE_HAVE_X86_64 1
#endif
#if defined(__linux__) && defined(__aarch64__)
#define RETRACE_HAVE_AARCH64 1
#endif

#ifdef __linux__
#include <sys/ptrace.h>
#include <sys/uio.h>
#ifdef RETRACE_HAVE_X86_64
#include <sys/user.h>
#endif
#ifdef RETRACE_HAVE_AARCH64
#include <asm/ptrace.h>
#endif
#endif

/* The kernel regset type we get from PTRACE_GETREGSET with NT_PRSTATUS. */
#ifdef RETRACE_HAVE_X86_64
typedef struct user_regs_struct retrace_x86_64_regs;
#endif
#ifdef RETRACE_HAVE_AARCH64
typedef struct user_pt_regs retrace_aarch64_regs;
#endif

retrace_ptrace_arch_t
retrace_ptrace_detect_arch(void)
{
#ifdef RETRACE_HAVE_X86_64
	return RETRACE_PTRACE_ARCH_X86_64;
#elif defined(RETRACE_HAVE_AARCH64)
	return RETRACE_PTRACE_ARCH_AARCH64;
#else
	return RETRACE_PTRACE_ARCH_UNKNOWN;
#endif
}

int
retrace_ptrace_read_regs(struct retrace_ptrace_frame *frame,
			 const void		     *regset_buf,
			 size_t			      regset_len)
{
	if (frame == NULL || regset_buf == NULL)
		return -1;

#ifdef RETRACE_HAVE_X86_64
	if (regset_len >= sizeof(retrace_x86_64_regs) &&
	    frame->arch == RETRACE_PTRACE_ARCH_X86_64) {
		const retrace_x86_64_regs *r = (const retrace_x86_64_regs *) regset_buf;

		frame->syscall_nr = (long) r->orig_rax;
		/* Syscall arg order on x86_64: rdi, rsi, rdx, r10, r8, r9.
		 * (rcx is clobbered by the syscall instruction; r10 stands
		 * in for arg3.)
		 */
		frame->arg_in[0] = r->rdi;
		frame->arg_in[1] = r->rsi;
		frame->arg_in[2] = r->rdx;
		frame->arg_in[3] = r->r10;
		frame->arg_in[4] = r->r8;
		frame->arg_in[5] = r->r9;
		frame->syscall_name =
		  retrace_ptrace_syscall_name(frame->arch, frame->syscall_nr);
		return 0;
	}
#endif

#ifdef RETRACE_HAVE_AARCH64
	if (regset_len >= sizeof(retrace_aarch64_regs) &&
	    frame->arch == RETRACE_PTRACE_ARCH_AARCH64) {
		const retrace_aarch64_regs *r = (const retrace_aarch64_regs *) regset_buf;

		/* On aarch64 x8 carries the syscall number; x0..x5 are args
		 * (x6, x7 unused for the standard 6-arg ABI).
		 */
		frame->syscall_nr = (long) r->regs[8];
		frame->arg_in[0] = r->regs[0];
		frame->arg_in[1] = r->regs[1];
		frame->arg_in[2] = r->regs[2];
		frame->arg_in[3] = r->regs[3];
		frame->arg_in[4] = r->regs[4];
		frame->arg_in[5] = r->regs[5];
		frame->syscall_name =
		  retrace_ptrace_syscall_name(frame->arch, frame->syscall_nr);
		return 0;
	}
#endif

	(void) regset_len;
	return -1;
}

int
retrace_ptrace_write_regs(void				    *regset_buf,
			  size_t			     regset_len,
			  const struct retrace_ptrace_frame *frame)
{
	if (regset_buf == NULL || frame == NULL)
		return -1;

#ifdef RETRACE_HAVE_X86_64
	if (regset_len >= sizeof(retrace_x86_64_regs) &&
	    frame->arch == RETRACE_PTRACE_ARCH_X86_64) {
		retrace_x86_64_regs *r = (retrace_x86_64_regs *) regset_buf;

		if (frame->arg_modified[0])
			r->rdi = frame->arg_out[0];
		if (frame->arg_modified[1])
			r->rsi = frame->arg_out[1];
		if (frame->arg_modified[2])
			r->rdx = frame->arg_out[2];
		if (frame->arg_modified[3])
			r->r10 = frame->arg_out[3];
		if (frame->arg_modified[4])
			r->r8 = frame->arg_out[4];
		if (frame->arg_modified[5])
			r->r9 = frame->arg_out[5];
		return 0;
	}
#endif

#ifdef RETRACE_HAVE_AARCH64
	if (regset_len >= sizeof(retrace_aarch64_regs) &&
	    frame->arch == RETRACE_PTRACE_ARCH_AARCH64) {
		retrace_aarch64_regs *r = (retrace_aarch64_regs *) regset_buf;

		if (frame->arg_modified[0])
			r->regs[0] = frame->arg_out[0];
		if (frame->arg_modified[1])
			r->regs[1] = frame->arg_out[1];
		if (frame->arg_modified[2])
			r->regs[2] = frame->arg_out[2];
		if (frame->arg_modified[3])
			r->regs[3] = frame->arg_out[3];
		if (frame->arg_modified[4])
			r->regs[4] = frame->arg_out[4];
		if (frame->arg_modified[5])
			r->regs[5] = frame->arg_out[5];
		return 0;
	}
#endif

	(void) regset_len;
	return -1;
}

int
retrace_ptrace_set_retval(void *regset_buf, size_t regset_len, long retval)
{
	if (regset_buf == NULL)
		return -1;

#ifdef RETRACE_HAVE_X86_64
	if (regset_len >= sizeof(retrace_x86_64_regs)) {
		retrace_x86_64_regs *r = (retrace_x86_64_regs *) regset_buf;
		/* On x86_64 the syscall return value is in rax. To force
		 * the kernel to deliver it without running the syscall,
		 * the caller also rewrites orig_rax to a no-op (e.g. -1)
		 * and the loop's PTRACE_SYSCALL continuation goes straight
		 * to syscall-exit.
		 */
		r->rax = (unsigned long long) retval;
		return 0;
	}
#endif

#ifdef RETRACE_HAVE_AARCH64
	if (regset_len >= sizeof(retrace_aarch64_regs)) {
		retrace_aarch64_regs *r = (retrace_aarch64_regs *) regset_buf;
		/* On aarch64 the syscall return value is x0. */
		r->regs[0] = (unsigned long) retval;
		return 0;
	}
#endif

	(void) regset_len;
	(void) retval;
	return -1;
}
