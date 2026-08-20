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
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Frame layouts for the Windows inline-hooking trampolines.
 *
 * Microsoft x64 ABI: integer/pointer args in rcx, rdx, r8, r9, with 32
 * bytes of shadow space above the return address. Floating-point and
 * vector args go in xmm0-xmm5 (not captured here; v2's v1 port doesn't
 * trace FP-only libc functions on Windows yet -- ADR-0010).
 *
 * arm64 (Windows on ARM): the AArch64 PCS is shared with POSIX AArch64
 * (x0-x7 integer args, v0-v7 FP). The same struct is reused; only the
 * register names differ.
 */

#ifndef RETRACE_BACKENDS_WIN_COMMON_FRAME_H
#define RETRACE_BACKENDS_WIN_COMMON_FRAME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Microsoft x64 ABI wrapper frame. The assembly trampoline pushes these
 * in this order so the C-side `retrace_engine_wrapper()` can read them.
 */
struct WrapperWinX64Frame {
	/* Set to 1 by the engine to make the trampoline call the real impl. */
	int64_t call_real_flag;

	/* Address of the real implementation trampoline (filled by the engine). */
	void *real_impl;

	/* Return value (filled by the engine when call_real_flag == 0). */
	int64_t ret_val;

	/* Original integer argument registers, as captured on entry. */
	uint64_t r9;
	uint64_t r8;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rsp;
};

/*
 * Windows-on-arm64 wrapper frame. Same shape as the POSIX AArch64 PCS
 * frame; kept as a distinct type for clarity (the trampoline may grow
 * Windows-specific FP register capture later).
 */
struct WrapperWinArm64Frame {
	int64_t call_real_flag;
	void  *real_impl;
	int64_t ret_val;

	/*
	 * x0-x7 in one array: the trampoline stores them contiguously
	 * (wrapper_arm64.S, [sp+24..80]) and win_arg() indexes them.
	 */
	uint64_t x[8];
	uint64_t sp;
};

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_BACKENDS_WIN_COMMON_FRAME_H */
