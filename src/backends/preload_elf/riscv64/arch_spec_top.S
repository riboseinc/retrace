/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Per-arch trampoline core for RISC-V rv64 (TODO.impl/20).
 *
 * The pattern is the mips64/aarch64 files'; the frame layout MUST
 * stay in sync with struct WrapperRiscv64Frame in
 * arch_spec_bottom.c.
 *
 * RISC-V rv64 (lp64d) ABI recap:
 *   - Integer/pointer args: $a0..$a7 (x10..x17)
 *   - FP args (doubles):    $fa0..$fa7 (f10..f17, 8 bytes each)
 *   - Return address:       $ra (x1); integer return: $a0;
 *     FP return: $fa0
 *   - Stack args begin at the caller's sp; sp stays 16-aligned
 *   - No delay slots; gp is fixed per module (set by the loader),
 *     so no cpsetup dance: the engine call is a plain PC-relative
 *     call (auipc/jalr against a hidden symbol folds statically)
 */

/*
 * Frame offsets as CPP constants: this file AND funcs_symbols.S
 * pass through cpp together (the .include-vs-#include lesson from
 * the MIPS port), so #define folds textually into immediates.
 */
#define OFFS_CALL_REAL_FLAG  0   /* long  */
#define OFFS_REAL_IMPL       8   /* void* */
#define OFFS_RET_VAL         16  /* 8 bytes: int or double bits */
#define OFFS_RET_IS_FP       24  /* int   */
	/* 28..31 padding */
#define OFFS_REAL_A0         32  /* a0..a7 (x10..x17), 8-byte stride */
#define OFFS_REAL_A1         40
#define OFFS_REAL_A2         48
#define OFFS_REAL_A3         56
#define OFFS_REAL_A4         64
#define OFFS_REAL_A5         72
#define OFFS_REAL_A6         80
#define OFFS_REAL_A7         88
#define OFFS_REAL_FA0        96  /* fa0..fa7 (f10..f17), 8-byte doubles */
#define OFFS_REAL_FA1        104
#define OFFS_REAL_FA2        112
#define OFFS_REAL_FA3        120
#define OFFS_REAL_FA4        128
#define OFFS_REAL_FA5        136
#define OFFS_REAL_FA6        144
#define OFFS_REAL_FA7        152
#define OFFS_REAL_RA         160 /* x1 at entry */
#define OFFS_ORIG_SP         168 /* entry sp: stack args (9+) */
#define FRAME_SIZE           192 /* 16-byte aligned */

/*
 * WRAPPER_ENTRY_RISCV64 func_name
 *
 * Emits two symbols (the interposed name and the wrapper):
 *   <func_name> / <func_name>_retrace_wrapper
 *
 * 1. Save $a0..$a7, $fa0..$fa7, $ra, and the entry sp.
 * 2. Zero the control fields.
 * 3. Call retrace_engine_wrapper(name, &frame).
 * 4. call_real_flag: restore regs, pop the frame, and jr to
 *    real_impl (tail call: its `ret` returns to the caller).
 *    Otherwise load the synthesized return from the frame.
 */
.macro WRAPPER_ENTRY_RISCV64 func_name
	.text
	/* 4-byte instruction alignment: the previous expansion ends
	 * with the .asciz name string, and nothing else guarantees
	 * the next entry lands on an instruction boundary.
	 */
	.align  2
	.global \func_name\()_retrace_wrapper
	.global \func_name
	.type   \func_name\()_retrace_wrapper, @function
	.type   \func_name, @function

\func_name:
\func_name\()_retrace_wrapper:
	/* allocate the frame; 192 keeps sp 16-aligned */
	addi    sp, sp, -FRAME_SIZE

	/* integer/pointer argument registers a0..a7 */
	sd      x10, OFFS_REAL_A0(sp)
	sd      x11, OFFS_REAL_A1(sp)
	sd      x12, OFFS_REAL_A2(sp)
	sd      x13, OFFS_REAL_A3(sp)
	sd      x14, OFFS_REAL_A4(sp)
	sd      x15, OFFS_REAL_A5(sp)
	sd      x16, OFFS_REAL_A6(sp)
	sd      x17, OFFS_REAL_A7(sp)

	/* FP argument registers fa0..fa7 (doubles) */
	fsd     f10, OFFS_REAL_FA0(sp)
	fsd     f11, OFFS_REAL_FA1(sp)
	fsd     f12, OFFS_REAL_FA2(sp)
	fsd     f13, OFFS_REAL_FA3(sp)
	fsd     f14, OFFS_REAL_FA4(sp)
	fsd     f15, OFFS_REAL_FA5(sp)
	fsd     f16, OFFS_REAL_FA6(sp)
	fsd     f17, OFFS_REAL_FA7(sp)

	/* return address + entry sp */
	sd      x1, OFFS_REAL_RA(sp)
	addi    x5, sp, FRAME_SIZE
	sd      x5, OFFS_ORIG_SP(sp)

	/* zero the control fields */
	sd      x0, OFFS_CALL_REAL_FLAG(sp)
	sd      x0, OFFS_REAL_IMPL(sp)
	sd      x0, OFFS_RET_VAL(sp)
	sw      x0, OFFS_RET_IS_FP(sp)

	/* C call: a0 = name string, a1 = frame.
	 * PC-relative load of the local .rodata name (the standard
	 * auipc/addi %pcrel_hi/%pcrel_lo pair). `call` expands to
	 * auipc/jalr: on a hidden symbol ld folds it statically, on
	 * a preemptible one through the PLT -- correct either way.
	 * x1 (ra) is ALREADY saved in the frame, so the call may
	 * clobber it.
	 */
1:
	auipc   a0, %pcrel_hi(\func_name\()_real_name)
	addi    a0, a0, %pcrel_lo(1b)
	addi    a1, sp, 0

	call    retrace_engine_wrapper

	/* reload call_real_flag */
	ld      x5, OFFS_CALL_REAL_FLAG(sp)
	beq     x5, x0, 1f

	/* tail call: restore args, pop, jump to the real impl */
	ld      x6, OFFS_REAL_IMPL(sp)
	ld      x10, OFFS_REAL_A0(sp)
	ld      x11, OFFS_REAL_A1(sp)
	ld      x12, OFFS_REAL_A2(sp)
	ld      x13, OFFS_REAL_A3(sp)
	ld      x14, OFFS_REAL_A4(sp)
	ld      x15, OFFS_REAL_A5(sp)
	ld      x16, OFFS_REAL_A6(sp)
	ld      x17, OFFS_REAL_A7(sp)
	fld     f10, OFFS_REAL_FA0(sp)
	fld     f11, OFFS_REAL_FA1(sp)
	fld     f12, OFFS_REAL_FA2(sp)
	fld     f13, OFFS_REAL_FA3(sp)
	fld     f14, OFFS_REAL_FA4(sp)
	fld     f15, OFFS_REAL_FA5(sp)
	fld     f16, OFFS_REAL_FA6(sp)
	fld     f17, OFFS_REAL_FA7(sp)
	ld      x1, OFFS_REAL_RA(sp)
	addi    sp, sp, FRAME_SIZE
	jr      x6

1:
	/* synthesized return: integer in a0, double bits in fa0 */
	lw      x5, OFFS_RET_IS_FP(sp)
	bne     x5, x0, 2f
	ld      x10, OFFS_RET_VAL(sp)
	j       3f
2:
	fld     f10, OFFS_RET_VAL(sp)
3:
	ld      x1, OFFS_REAL_RA(sp)
	addi    sp, sp, FRAME_SIZE
	ret

	/* the name lives in .rodata: shared, never written */
	.section .rodata
\func_name\()_real_name:
	.asciz  "\func_name"
	.size   \func_name\()_real_name, . - \func_name\()_real_name
	.endm
