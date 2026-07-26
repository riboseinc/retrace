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
 * Syscall number -> name tables for the ptrace backend.
 *
 * The ptrace backend intercepts at the kernel syscall boundary, so it
 * speaks syscall numbers, not libc symbol addresses. These tables map
 * the numbers reported by PTRACE_GETREGSET (orig_rax on x86_64, x8 on
 * aarch64) to the libc function names the engine already knows
 * (write/read/open/close/...). Per-arch: x86_64 and aarch64 use
 * different numbering (see <asm/unistd.h>).
 *
 * Adding a new syscall is additive: append to the relevant table in
 * syscall_table.c. Engine code never changes.
 */

#ifndef RETRACE_BACKENDS_PTRACE_SYSCALL_TABLE_H
#define RETRACE_BACKENDS_PTRACE_SYSCALL_TABLE_H

#include <stddef.h>

/* Architectures recognised by the ptrace backend's register translation. */
typedef enum {
	RETRACE_PTRACE_ARCH_X86_64 = 0,
	RETRACE_PTRACE_ARCH_AARCH64 = 1,
	RETRACE_PTRACE_ARCH_UNKNOWN = -1,
} retrace_ptrace_arch_t;

/* Single table entry: syscall number + canonical libc name. */
struct retrace_ptrace_syscall_entry {
	long	    number;
	const char *name;
};

/*
 * Look up the syscall name for `number` on `arch`. Returns NULL if the
 * number is not in the table (unknown / untraced syscall).
 */
const char *retrace_ptrace_syscall_name(retrace_ptrace_arch_t arch, long number);

/*
 * Look up the syscall number for `name` on `arch`. Returns -1 if the
 * name is not in the table.
 */
long retrace_ptrace_syscall_number(retrace_ptrace_arch_t arch, const char *name);

/* Iterate a per-arch table. Returns entry count; *out points at the
 * first entry (NULL if arch is unknown).
 */
size_t retrace_ptrace_syscall_table(retrace_ptrace_arch_t			arch,
				    const struct retrace_ptrace_syscall_entry **out);

#endif /* RETRACE_BACKENDS_PTRACE_SYSCALL_TABLE_H */
