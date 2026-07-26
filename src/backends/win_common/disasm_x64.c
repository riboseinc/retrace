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
 * See disasm.h for the design and conservative v1 policy.
 */

#include "disasm.h"

/*
 * Decode the ModR/M byte and return the extra bytes it implies
 * (SIB + displacement). Returns -1 if the instruction is unsafe
 * (RIP-relative addressing), or the displacement size (0, 1, 4) on success.
 *
 * ModR/M layout: [mod(2)][reg/opcode(3)][r/m(3)]
 *   mod=00:
 *     r/m=101 -> RIP+disp32 (UNSAFE for relocation; refuse)
 *     r/m=100 -> SIB byte follows, no disp
 *     else    -> no displacement
 *   mod=01:
 *     r/m=100 -> SIB + disp8
 *     else    -> disp8
 *   mod=10:
 *     r/m=100 -> SIB + disp32
 *     else    -> disp32
 *   mod=11: register-direct, no disp
 */
static int
modrm_extra(const unsigned char *p, size_t avail, int *sib_out)
{
	unsigned char modrm;
	unsigned char mod, rm;
	int has_sib = 0;
	int disp = 0;

	if (avail < 1)
		return -1;

	modrm = p[0];
	mod = (modrm >> 6) & 0x3;
	rm = modrm & 0x7;

	switch (mod) {
	case 0:
		if (rm == 5) {
			/* RIP-relative addressing -- unsafe to relocate. */
			return -1;
		}
		if (rm == 4) {
			has_sib = 1;
		}
		break;
	case 1:
		if (rm == 4)
			has_sib = 1;
		disp = 1;
		break;
	case 2:
		if (rm == 4)
			has_sib = 1;
		disp = 4;
		break;
	case 3:
	default:
		break;
	}

	*sib_out = has_sib;
	return disp;
}

/*
 * Returns the length of the instruction starting at `code`, or 0 if
 * unrecognized / unsafe. The returned length includes all prefixes,
 * opcode, ModR/M, SIB, displacement, and immediate bytes.
 *
 * Conservative: anything that looks like a relative branch/call returns 0.
 */
static size_t
decode_one(const unsigned char *code, size_t avail)
{
	size_t len = 0;
	unsigned char op;
	int has_rex_w = 0;
	unsigned char rex = 0;

	/* Legacy prefixes (we don't expect these in prologues, but be safe). */
	while (len < avail) {
		op = code[len];
		if (op == 0xF0 || op == 0xF2 || op == 0xF3 ||
		    op == 0x2E || op == 0x36 || op == 0x3E ||
		    op == 0x26 || op == 0x64 || op == 0x65 || op == 0x66) {
			len++;
			continue;
		}
		break;
	}
	if (len >= avail)
		return 0;

	/* REX prefix (0x40-0x4F). */
	if (code[len] >= 0x40 && code[len] <= 0x4F) {
		rex = code[len];
		has_rex_w = (rex & 0x8) != 0;
		len++;
		if (len >= avail)
			return 0;
	}

	op = code[len];
	len++;

	/* 1-byte instructions that take no operands. */
	if (op == 0x90 /* nop */ || op == 0xC3 /* ret */ || op == 0xCC /* int3 */ ||
	    op == 0xC9 /* leave */) {
		return len;
	}

	/* push/pop register: 50-5F (1 byte total, no REX needed for r8-r15
	 * REX.B flips the register).
	 */
	if (op >= 0x50 && op <= 0x5F) {
		return len;
	}

	/* Relative branches -- UNSAFE to relocate. */
	if (op == 0xE8 /* call rel32 */ || op == 0xE9 /* jmp rel32 */) {
		return 0;
	}
	if (op == 0xEB /* jmp rel8 */ ||
	    (op >= 0xE0 && op <= 0xE3) /* loop/jcxz */) {
		return 0;
	}
	if (op == 0x70 || op == 0x71 || op == 0x72 || op == 0x73 ||
	    op == 0x74 || op == 0x75 || op == 0x76 || op == 0x77 ||
	    op == 0x78 || op == 0x79 || op == 0x7A || op == 0x7B ||
	    op == 0x7C || op == 0x7D || op == 0x7E || op == 0x7F) {
		/* Jcc rel8 -- unsafe. */
		return 0;
	}
	if (op == 0x0F) {
		/* Two-byte opcode 0F xx -- includes Jcc rel32 (0F 80-8F) which
		 * is unsafe. Conservatively refuse all 0F-prefixed instructions
		 * in the prologue window.
		 */
		return 0;
	}

	/* FF /2 (call r/m), FF /4 (jmp r/m), FF /3 (callf) -- refuse to be
	 * safe (an indirect call through a register could be `call qword ptr
	 * [rip+x]` which is unsafe).
	 */
	if (op == 0xFF) {
		return 0;
	}

	/* mov r64, imm64: REX.W + B8-BF (mov reg, imm64). 8 bytes immediate. */
	if (has_rex_w && op >= 0xB8 && op <= 0xBF) {
		if (len + 8 > avail)
			return 0;
		return len + 8;
	}

	/* mov r/m, imm32: C7 /0. ModR/M + (SIB) + disp + imm32. */
	if (op == 0xC7) {
		int sib = 0;
		int disp;

		if (len >= avail)
			return 0;
		disp = modrm_extra(code + len, avail - len, &sib);
		if (disp < 0)
			return 0;
		len += 1 + (sib ? 1 : 0) + (size_t)disp;
		if (len + 4 > avail)
			return 0;
		return len + 4;
	}

	/* sub/add/cmp r/m, imm8: 83 /5,83 /0,83 /7 etc. ModR/M only (or +SIB/disp). */
	if (op == 0x83) {
		int sib = 0;
		int disp;

		if (len >= avail)
			return 0;
		disp = modrm_extra(code + len, avail - len, &sib);
		if (disp < 0)
			return 0;
		len += 1 + (sib ? 1 : 0) + (size_t)disp;
		if (len + 1 > avail)
			return 0;
		return len + 1;
	}

	/* sub/add r/m, imm32: 81 /5, 81 /0. */
	if (op == 0x81) {
		int sib = 0;
		int disp;

		if (len >= avail)
			return 0;
		disp = modrm_extra(code + len, avail - len, &sib);
		if (disp < 0)
			return 0;
		len += 1 + (sib ? 1 : 0) + (size_t)disp;
		if (len + 4 > avail)
			return 0;
		return len + 4;
	}

	/* mov r/m, r and r, r/m: 88, 89, 8A, 8B. ModR/M-based. */
	if (op == 0x88 || op == 0x89 || op == 0x8A || op == 0x8B) {
		int sib = 0;
		int disp;

		if (len >= avail)
			return 0;
		disp = modrm_extra(code + len, avail - len, &sib);
		if (disp < 0)
			return 0;
		len += 1 + (sib ? 1 : 0) + (size_t)disp;
		return len;
	}

	/* lea r64, m: 8D. ModR/M-based (always memory operand). */
	if (op == 0x8D) {
		int sib = 0;
		int disp;

		if (len >= avail)
			return 0;
		disp = modrm_extra(code + len, avail - len, &sib);
		if (disp < 0)
			return 0;
		len += 1 + (sib ? 1 : 0) + (size_t)disp;
		return len;
	}

	/* mov r8, imm8 (B0-B7), mov r32, imm32 (B8-BF without REX.W). */
	if (op >= 0xB0 && op <= 0xB7) {
		if (len + 1 > avail)
			return 0;
		return len + 1;
	}
	if (op >= 0xB8 && op <= 0xBF) {
		/* Without REX.W the immediate is 4 bytes (mov r32, imm32). */
		if (len + 4 > avail)
			return 0;
		return len + 4;
	}

	/* xchg eax, r32 (90 with no REX would be nop; handled above).
	 * push imm8 (6A), push imm32 (68).
	 */
	if (op == 0x6A) {
		if (len + 1 > avail)
			return 0;
		return len + 1;
	}
	if (op == 0x68) {
		if (len + 4 > avail)
			return 0;
		return len + 4;
	}

	/* Unrecognized opcode -- refuse. */
	return 0;
}

size_t
retrace_disasm_x64_prologue_len(const unsigned char *code,
				 size_t min_bytes,
				 size_t max_scan)
{
	size_t total = 0;

	while (total < min_bytes) {
		size_t one;

		if (total >= max_scan)
			return 0;
		one = decode_one(code + total, max_scan - total);
		if (one == 0)
			return 0;
		total += one;
	}

	return total;
}
