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
 * AArch64 (LP64) trampoline for the Mach-O backend (macOS / Apple Silicon).
 *
 * The frame layout is identical to the ELF variant (and to
 * struct WrapperAArch64Frame in arch_spec_bottom.c).
 *
 * Mach-O differences from the ELF variant:
 *   - All external/global symbols are prefixed with an underscore.
 *   - The function-name table goes into __DATA,__retrace_rimpls (one entry
 *     per function: fixed-width name + pointer to the real symbol) so the
 *     resolver in arch_spec_bottom.c can walk it.
 *   - The DYLD interpose array goes into __DATA,__interpose
 *     (replacer, replaced) pairs.
 *   - Local labels use the `L` prefix so they are stripped from the symbol
 *     table.
 */

	/* This must equal MAXLEN_FUNC_NAME from src/core/funcs.h. */
	.set MAXLEN_FUNC_NAME, 64

	/* control fields */
	.set OFFS_CALL_REAL_FLAG, 0
	.set OFFS_REAL_IMPL,      8
	.set OFFS_RET_VAL,        16    /* union { long i; __uint128_t fp; } */
	.set OFFS_RET_IS_FP,      32
	/* 36..39 padding */
	.set OFFS_REAL_X0,        40
	.set OFFS_REAL_X1,        48
	.set OFFS_REAL_X2,        56
	.set OFFS_REAL_X3,        64
	.set OFFS_REAL_X4,        72
	.set OFFS_REAL_X5,        80
	.set OFFS_REAL_X6,        88
	.set OFFS_REAL_X7,        96
	/* 104..111 padding to align SIMD region to 16 */
	.set OFFS_REAL_V0,        112
	.set OFFS_REAL_V1,        128
	.set OFFS_REAL_V2,        144
	.set OFFS_REAL_V3,        160
	.set OFFS_REAL_V4,        176
	.set OFFS_REAL_V5,        192
	.set OFFS_REAL_V6,        208
	.set OFFS_REAL_V7,        224
	.set OFFS_REAL_LR,        240
	.set OFFS_ORIG_SP,        248
	.set FRAME_SIZE,          256

	.section __TEXT, __text

.macro WRAPPER_ENTRY_AARCH64 func_name
	.section __TEXT, __text
	.global _\func_name\()_retrace_wrapper

	/* NOTE: we deliberately do NOT define _\func_name as a local symbol.
	 * On macOS, _\func_name must remain an EXTERNAL reference to the real
	 * libc/libsystem function. This is critical for two reasons:
	 *
	 *  1. The __DATA,__interpose entry needs {replacer=our_wrapper,
	 *     replacee=real_func}. If _\func_name were local, the linker
	 *     would resolve it to our wrapper's address, making the interpose
	 *     entry a no-op ({wrapper, wrapper}).
	 *
	 *  2. The __DATA,__retrace_rimpls pointer (.quad _\func_name) must
	 *     point to the REAL function so retrace_as_get_real_safe() can
	 *     call it without infinite recursion through the wrapper.
	 *
	 * dyld processes __interpose entries at load time and rebinds other
	 * images' references to _\func_name → _\func_name_retrace_wrapper.
	 * References within this image (including __retrace_rimpls) are
	 * left untouched by dyld's interpose pass, so they correctly see
	 * the real function. */
_\func_name\()_retrace_wrapper:
	sub     sp, sp, #FRAME_SIZE

	stp     x0, x1, [sp, #OFFS_REAL_X0]
	stp     x2, x3, [sp, #OFFS_REAL_X2]
	stp     x4, x5, [sp, #OFFS_REAL_X4]
	stp     x6, x7, [sp, #OFFS_REAL_X6]

	stp     q0, q1, [sp, #OFFS_REAL_V0]
	stp     q2, q3, [sp, #OFFS_REAL_V2]
	stp     q4, q5, [sp, #OFFS_REAL_V4]
	stp     q6, q7, [sp, #OFFS_REAL_V6]

	str     x30, [sp, #OFFS_REAL_LR]
	add     x9, sp, #FRAME_SIZE
	str     x9,  [sp, #OFFS_ORIG_SP]

	stp     xzr, xzr, [sp, #OFFS_CALL_REAL_FLAG]
	stp     xzr, xzr, [sp, #OFFS_RET_VAL]
	str     wzr,     [sp, #OFFS_RET_IS_FP]

	/* x0 = pointer to func name string, x1 = &frame */
	adrp    x0, L\func_name\()_retrace_real_name@PAGE
	add     x0, x0, #L\func_name\()_retrace_real_name@PAGEOFF
	add     x1, sp, #0

	bl      _retrace_engine_wrapper

	ldr     x9, [sp, #OFFS_CALL_REAL_FLAG]
	cbz     x9, 1f

	ldr     x16, [sp, #OFFS_REAL_IMPL]

	ldp     x0, x1, [sp, #OFFS_REAL_X0]
	ldp     x2, x3, [sp, #OFFS_REAL_X2]
	ldp     x4, x5, [sp, #OFFS_REAL_X4]
	ldp     x6, x7, [sp, #OFFS_REAL_X6]

	ldp     q0, q1, [sp, #OFFS_REAL_V0]
	ldp     q2, q3, [sp, #OFFS_REAL_V2]
	ldp     q4, q5, [sp, #OFFS_REAL_V4]
	ldp     q6, q7, [sp, #OFFS_REAL_V6]

	ldr     x30, [sp, #OFFS_REAL_LR]
	add     sp, sp, #FRAME_SIZE
	br      x16

1:
	ldr     w17, [sp, #OFFS_RET_IS_FP]
	cbnz    w17, 2f
	ldr     x0, [sp, #OFFS_RET_VAL]
	b       3f
2:
	ldr     q0, [sp, #OFFS_RET_VAL]
3:
	ldr     x30, [sp, #OFFS_REAL_LR]
	add     sp, sp, #FRAME_SIZE
	ret

	/* The real-name table: each entry is MAXLEN_FUNC_NAME bytes of name
	 * followed by the address of the real symbol. The resolver walks this
	 * to find a non-interposed pointer to the original libc symbol. */
	.section __DATA, __retrace_rimpls
L\func_name\()_retrace_real_name:
	.asciz  "\func_name"
	.set    L\func_name\()_retrace_real_name_end, . - L\func_name\()_retrace_real_name
	.zero   MAXLEN_FUNC_NAME - (L\func_name\()_retrace_real_name_end)
	.quad   _\func_name

	/* DYLD interpose entry: { replacer, replaced }. */
	.section __DATA, __interpose
	.quad   _\func_name\()_retrace_wrapper
	.quad   _\func_name

	.endm
