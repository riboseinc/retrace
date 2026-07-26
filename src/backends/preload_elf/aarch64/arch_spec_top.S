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
 * AArch64 (LP64) trampoline for the ELF backend (Linux, etc.).
 *
 * The frame layout below MUST stay in sync with struct WrapperAArch64Frame
 * in arch_spec_bottom.c. Offsets are documented inline.
 *
 * AArch64 PCS recap:
 *   - Integer/pointer args: x0..x7 (8 registers)
 *   - Floating/SIMD args:   v0..v7 (8 SIMD registers, each 128-bit)
 *   - Stack args begin above the caller's frame; sp at entry points to
 *     the lowest-occupied stack argument (16-byte aligned at all times).
 *   - Return address in x30 (lr); integer return in x0, FP return in v0.
 */

	/* control fields */
	.set OFFS_CALL_REAL_FLAG, 0     /* long  */
	.set OFFS_REAL_IMPL,      8     /* void* */
	.set OFFS_RET_VAL,        16    /* union { long i; __uint128_t fp; } (16 bytes) */
	.set OFFS_RET_IS_FP,      32    /* int   */
	/* 36..39 padding */
	/* saved integer arg registers x0..x7 (8-byte stride, 8-aligned) */
	.set OFFS_REAL_X0,        40
	.set OFFS_REAL_X1,        48
	.set OFFS_REAL_X2,        56
	.set OFFS_REAL_X3,        64
	.set OFFS_REAL_X4,        72
	.set OFFS_REAL_X5,        80
	.set OFFS_REAL_X6,        88
	.set OFFS_REAL_X7,        96
	/* 104..111 padding to bring SIMD region onto a 16-byte boundary
	 * (stp/ldp of Q registers require the offset to be a multiple of 16) */
	/* saved SIMD/FP arg registers v0..v7 (each 16 bytes, 16-aligned) */
	.set OFFS_REAL_V0,        112
	.set OFFS_REAL_V1,        128
	.set OFFS_REAL_V2,        144
	.set OFFS_REAL_V3,        160
	.set OFFS_REAL_V4,        176
	.set OFFS_REAL_V5,        192
	.set OFFS_REAL_V6,        208
	.set OFFS_REAL_V7,        224
	/* link register (x30) as seen at entry */
	.set OFFS_REAL_LR,        240
	/* sp at entry (lowest stack arg, used to walk variadic/stack args) */
	.set OFFS_ORIG_SP,        248
	.set FRAME_SIZE,          256

	.text

/*
 * WRAPPER_ENTRY_AARCH64 func_name
 *
 * Emits two symbols:
 *   <func_name>             -- the interposed symbol (weak alias of the wrapper)
 *   <func_name>_retrace_wrapper -- the trampoline entry
 *
 * The wrapper:
 *   1. Saves x0..x7, v0..v7, x30 (lr), and the entry sp into the frame.
 *   2. Zeroes control fields (call_real_flag, real_impl, ret_val, ret_is_fp).
 *   3. Calls retrace_engine_wrapper(func_name_str, &frame).
 *   4. If call_real_flag != 0: restore regs and tail-call (branch, not call)
 *      real_impl so the real function's `ret` returns straight to the caller.
 *      Otherwise: load the (possibly FP) return value from the frame and
 *      return to the caller via x30.
 *
 * Stack stays 16-byte aligned at every call boundary: FRAME_SIZE is 256.
 */
.macro WRAPPER_ENTRY_AARCH64 func_name
	.text
	.global \func_name\()_retrace_wrapper
	.global \func_name
	.type   \func_name\()_retrace_wrapper, @function
	.type   \func_name, @function

\func_name:
\func_name\()_retrace_wrapper:
	/* Allocate the frame. 256 B keeps sp 16-aligned. */
	sub     sp, sp, #FRAME_SIZE

	/* Save the integer argument registers x0..x7. */
	stp     x0, x1, [sp, #OFFS_REAL_X0]
	stp     x2, x3, [sp, #OFFS_REAL_X2]
	stp     x4, x5, [sp, #OFFS_REAL_X4]
	stp     x6, x7, [sp, #OFFS_REAL_X6]

	/* Save the SIMD/FP argument registers v0..v7 (Q regs, 128-bit). */
	stp     q0, q1, [sp, #OFFS_REAL_V0]
	stp     q2, q3, [sp, #OFFS_REAL_V2]
	stp     q4, q5, [sp, #OFFS_REAL_V4]
	stp     q6, q7, [sp, #OFFS_REAL_V6]

	/* Save lr (x30) and the caller's sp.
	 * At entry the caller had already pushed stack args; sp points at
	 * the lowest stack-occupied argument, which is where params 9+ live.
	 * Orig sp = (sp at entry) = (sp now) + FRAME_SIZE. */
	str     x30, [sp, #OFFS_REAL_LR]
	add     x9, sp, #FRAME_SIZE
	str     x9,  [sp, #OFFS_ORIG_SP]

	/* Zero the control fields: call_real_flag, real_impl, ret_val[0..15],
	 * ret_is_fp. They are filled by the intercept logic if needed. */
	stp     xzr, xzr, [sp, #OFFS_CALL_REAL_FLAG]   /* flag + real_impl */
	stp     xzr, xzr, [sp, #OFFS_RET_VAL]          /* ret_val[0..15]  */
	str     wzr,     [sp, #OFFS_RET_IS_FP]         /* ret_is_fp = 0   */

	/* Set up the C call: x0 = func_name string, x1 = &frame. */
	adrp    x0, \func_name\()_real_name
	add     x0, x0, :lo12:\func_name\()_real_name
	add     x1, sp, #0

	/* retrace_engine_wrapper is a C function and may clobber x0..x18 and v0..v31
	 * (the saved frame already captured everything we care about). It obeys
	 * AAPCS so lr is preserved across the call. */
	bl      retrace_engine_wrapper

	/* Reload call_real_flag. */
	ldr     x9, [sp, #OFFS_CALL_REAL_FLAG]
	cbz     x9, 1f

	/* call_real_flag != 0: restore arg regs and tail-call real_impl.
	 * The real function's `ret` returns directly to the original caller
	 * (x30 was saved in the frame and we restore it here). */
	ldr     x16, [sp, #OFFS_REAL_IMPL]   /* branch target */

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
	/* call_real_flag == 0: the intercept already chose the return value.
	 * Load either x0 (integer) or v0 (FP) from the frame. */
	ldr     w17, [sp, #OFFS_RET_IS_FP]
	cbnz    w17, 2f

	ldr     x0, [sp, #OFFS_RET_VAL]
	b       3f

2:
	ldr     q0, [sp, #OFFS_RET_VAL]

3:
	/* Restore the original lr (which is also where we return) and free the
	 * frame. The original x0..x7/v0..v7 do NOT need restoring here —
	 * the intercept dictated the return value above, and per AAPCS the
	 * caller only observes x0/v0. */
	ldr     x30, [sp, #OFFS_REAL_LR]
	add     sp, sp, #FRAME_SIZE
	ret

	/* The function name string lives in .rodata so it is shared across
	 * processes and never written. */
	.section .rodata
\func_name\()_real_name:
	.asciz  "\func_name"
	.size   \func_name\()_real_name, . - \func_name\()_real_name
	.endm
