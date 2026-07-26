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
 * arm64 prologue length decoder. All arm64 instructions are 4 bytes; the
 * prologue relocation window must therefore be a multiple of 4 bytes
 * >= min_bytes (the patch size, which is 16 for arm64).
 *
 * No instruction-level safety check is performed -- arm64 prologues for
 * typical libc functions are stp/sp-movement sequences that are safe to
 * relocate. A full arm64 disassembler (looking for ADRP-relative loads
 * or PC-relative branches in the prologue) would be a future improvement.
 */

#include "disasm.h"

size_t
retrace_disasm_arm64_prologue_len(const unsigned char *code,
				  size_t min_bytes,
				  size_t max_scan)
{
	size_t rounded;

	(void)code;

	if (min_bytes == 0 || min_bytes > max_scan)
		return 0;

	/* Round up to next 4-byte boundary. */
	rounded = (min_bytes + 3u) & ~(size_t)3u;
	if (rounded > max_scan)
		return 0;

	return rounded;
}
