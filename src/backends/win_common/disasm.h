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
 * Minimal x64 instruction-length decoder for prologue relocation.
 *
 * Goal: given a target function's first bytes, find the smallest N >=
 * `min_bytes` such that bytes [0, N) contain only whole instructions that
 * are safe to relocate (no relative branches, no RIP-relative addressing
 * in the relocated window).
 *
 * Conservative v1 policy (ADR-0009): if any instruction in the window is
 * a relative branch/call (E8/E9 rel32, EB rel8, E0-E3, E9Cb, FF /2, FF /3),
 * or uses RIP-relative ModR/M addressing (ModR/M mod=00 r/m=101), the
 * function is refused.
 *
 * This decoder handles only the common MSVC prologue patterns:
 *   - 1-byte: push reg (50-5F), ret (C3), nop (90), int3 (CC)
 *   - 1-byte prefixes: REX (40-4F)
 *   - 2-byte: mov r64, imm64 (48 B8-BF + 8 bytes), mov r/m64, r64 (48 89 ...)
 *   - 3-byte: sub rsp, imm8 (48 83 EC ib), add rsp, imm8 (48 83 C4 ib)
 *   - sub rsp, imm32 (48 81 EC id), mov [rsp+disp8], reg (48 89 4C 24 ib)
 *   - lea rbp, [rsp+disp8] (48 8D 6C 24 ib)
 *
 * Anything not recognized returns -1 (refuse-to-hook).
 */

#ifndef RETRACE_BACKENDS_WIN_COMMON_DISASM_H
#define RETRACE_BACKENDS_WIN_COMMON_DISASM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Decode instructions starting at `code`, accumulating whole instructions
 * until at least `min_bytes` are covered. Returns the total length of the
 * decoded instructions (>= min_bytes), or 0 if any instruction is unsafe
 * or unrecognized before reaching `min_bytes`.
 *
 * `max_scan` is the upper bound on how many bytes we are willing to read.
 */
size_t retrace_disasm_x64_prologue_len(const unsigned char *code,
				       size_t min_bytes,
				       size_t max_scan);

/* arm64: trivial -- all instructions are 4 bytes. Round up to a multiple
 * of 4 that is >= min_bytes. No safety check is possible without a full
 * decoder; the caller is responsible for ensuring arm64 prologues are
 * safe (they are, for the typical stp/sp movement).
 */
size_t retrace_disasm_arm64_prologue_len(const unsigned char *code,
					 size_t min_bytes,
					 size_t max_scan);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_BACKENDS_WIN_COMMON_DISASM_H */
