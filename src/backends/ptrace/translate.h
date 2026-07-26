/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided the following conditions
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
 * Register-level translation between ptrace's PTRACE_GETREGSET regset
 * and the engine's syscall frame.
 *
 * Syscall arg order differs from the function-call ABI on x86_64:
 *   call conv:  rdi, rsi, rdx, rcx, r8,  r9
 *   syscall:    rdi, rsi, rdx, r10, r8,  r9     (rcx clobbered -> r10)
 * aarch64 uses x0..x7 for both, so no swap.
 *
 * The frame here is the small value the trace loop hands to
 * retrace_engine_wrapper(); translate.c is the single source of truth
 * for how registers map to/from engine args.
 */

#ifndef RETRACE_BACKENDS_PTRACE_TRANSLATE_H
#define RETRACE_BACKENDS_PTRACE_TRANSLATE_H

#include "syscall_table.h"

#include <stddef.h>

/* Maximum syscall args the engine examines for any one call. */
#define RETRACE_PTRACE_MAX_ARGS 6

/*
 * Portable syscall frame. The trace loop fills this from raw registers
 * on syscall entry, hands it to retrace_engine_wrapper(), then writes
 * any modifications back via PTRACE_SETREGSET.
 *
 * The frame is intentionally plain data so that translate.c has no
 * dependency on engine internals.
 */
struct retrace_ptrace_frame {
	retrace_ptrace_arch_t arch;

	/* Syscall number (orig_rax on x86_64, x8 on aarch64). */
	long syscall_nr;

	/* Canonical libc name for syscall_nr, or NULL if unknown. */
	const char *syscall_name;

	/* Input args 0..5 as read from registers. */
	unsigned long arg_in[RETRACE_PTRACE_MAX_ARGS];

	/* Output args to write back. translate applies only entries with
	 * the corresponding arg_modified bit set.
	 */
	unsigned long arg_out[RETRACE_PTRACE_MAX_ARGS];
	unsigned char arg_modified[RETRACE_PTRACE_MAX_ARGS];

	/* Engine-requested skip: do not run the real syscall, instead set
	 * return value and jump to syscall-exit.
	 */
	int  skip_real;
	long forced_retval;
};

/* Detect the host's ptrace architecture at runtime. Returns
 * RETRACE_PTRACE_ARCH_UNKNOWN on hosts without ptrace support.
 */
retrace_ptrace_arch_t retrace_ptrace_detect_arch(void);

/* Fill `frame` from a raw regset blob obtained via PTRACE_GETREGSET.
 * `regset_buf` points at the iov_base buffer; `regset_len` is its
 * length. Returns 0 on success, -1 on size/arch mismatch.
 */
int retrace_ptrace_read_regs(struct retrace_ptrace_frame *frame,
			     const void			 *regset_buf,
			     size_t			  regset_len);

/* Write any arg_out modifications back into the same regset blob.
 * `regset_buf` is the iov_base buffer that will be passed to
 * PTRACE_SETREGSET; `regset_len` is its capacity. Returns 0 on
 * success, -1 if the buffer is too small.
 */
int retrace_ptrace_write_regs(void				*regset_buf,
			      size_t				 regset_len,
			      const struct retrace_ptrace_frame *frame);

/* Set the return-value register inside `regset_buf` to `retval`.
 * Used when the engine asks the loop to skip the real syscall.
 */
int retrace_ptrace_set_retval(void *regset_buf, size_t regset_len, long retval);

#endif /* RETRACE_BACKENDS_PTRACE_TRANSLATE_H */
