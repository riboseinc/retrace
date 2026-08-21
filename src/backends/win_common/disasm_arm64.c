/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice and this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * arm64 prologue decoder (TODO.trace-profile/12). All arm64
 * instructions are 4 bytes; the relocation window is a multiple
 * of 4 >= min_bytes (16 for the arm64 patch).
 *
 * ALLOWLIST, mirroring the x64 refuse-unsafe philosophy: a
 * copied instruction must be provably position-independent.
 * The v1 "trust typical prologues" stub relocated ADRP/branches
 * blindly -- an adrp in the first 16 bytes of ucrtbase getenv
 * made the trampoline compute a wrong page address and the
 * pass-through segfaulted (windows-11-arm, v2.16.0 attempt).
 *
 * Safe here: pair loads/stores, add/sub immediates, movz/movn/
 * movk, orr-immediate (incl. the mov alias), register-base
 * (non-literal) loads/stores, the pointer-auth hints, nop.
 * Everything else -- adrp/adr, any branch, pc-relative literals,
 * system ops -- REFUSES the hook (function runs uninstrumented).
 */

#include "disasm.h"

#include <stdint.h>
#include <string.h>

static int insn_is_relocatable(uint32_t insn)
{
	/* paciasp / pacibsp (pointer-auth hints) */
	if (insn == 0xD503233FU || insn == 0xD503237FU)
		return 1;
	/* nop */
	if (insn == 0xD503201FU)
		return 1;
	/* ldp/stp group (signed-offset and pre/post-index) */
	if ((insn & 0x36000000U) == 0x20000000U)
		return 1;
	/* add/subtract immediate (sub sp, sp, #N) */
	if ((insn & 0x1F000000U) == 0x11000000U ||
	    (insn & 0x1F000000U) == 0x51000000U)
		return 1;
	/* movn/movz/movk (wide immediates) */
	if ((insn & 0x7F800000U) == 0x12800000U ||
	    (insn & 0x7F800000U) == 0x52800000U ||
	    (insn & 0x7F800000U) == 0x72800000U)
		return 1;
	/* orr immediate (alias encodings of reg-reg mov) */
	if ((insn & 0x1F800000U) == 0x32000000U)
		return 1;
	/* mov (register) alias: orr Rd, xzr, Rn */
	if ((insn & 0x7FE0FC00U) == 0x2A0003E0U ||
	    (insn & 0x7FE0FC00U) == 0xAA0003E0U)
		return 1;
	/*
	 * Register-base (non-literal, non-pair) loads/stores: the
	 * unscaled/pre-post-index group (0x38 -- e.g. str x19,
	 * [sp, #-16]!) AND the unsigned-offset group (0x39 -- e.g.
	 * str x25, [sp, #32]); mask 0x3A covers both while keeping
	 * the pair (0x28) and PC-relative literal (0x18) forms out.
	 * All register-addressed, hence position-independent.
	 */
	if ((insn & 0x3A000000U) == 0x38000000U)
		return 1;
	return 0;
}

size_t
retrace_disasm_arm64_prologue_len(const unsigned char *code,
				  size_t min_bytes,
				  size_t max_scan)
{
	size_t rounded;
	size_t off;

	if (code == NULL || min_bytes == 0 || min_bytes > max_scan)
		return 0;

	/* Round up to the next whole-instruction boundary. */
	rounded = (min_bytes + 3u) & ~(size_t)3u;
	if (rounded > max_scan)
		return 0;

	for (off = 0; off < rounded; off += 4) {
		uint32_t insn;

		memcpy(&insn, code + off, sizeof(insn));
		if (!insn_is_relocatable(insn))
			return 0;
	}
	return rounded;
}
