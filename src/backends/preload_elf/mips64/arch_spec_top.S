/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Per-arch trampoline core for MIPS n64 (TODO.impl/19).
 *
 * The pattern is the aarch64 file's (ADR-0010: FP args saved
 * from day one); the frame layout MUST stay in sync with struct
 * WrapperMips64Frame in arch_spec_bottom.c.
 *
 * MIPS n64 ABI recap (o64/n64 SGI framing):
 *   - Integer/pointer args: $a0..$a7 ($4..$11)
 *   - FP args (doubles):    $f12..$f19 (one per arg slot)
 *   - Return address: $ra ($31); function address for PIC: $t9
 *   - Integer return: $v0 (and $v1 for the second word);
 *     FP return: $f0 ($f1 holds the odd half of long doubles)
 *   - Stack args begin at the caller's sp; sp stays 16-aligned
 *   - Branches have DELAY SLOTS: .set noreorder + explicit
 *     nops everywhere a delay slot is not deliberately used
 */

	/* self-describing ISA: the cross driver may assemble in a
	 * default 32-bit mode; the trampoline IS n64 from its first
	 * instruction. The .module forms are the modern gas ones
	 * (the .set twins do not drive the FP register-bank model
	 * -- the Debian cross-as lesson: sdc1 $f13 assembles only
	 * under .module oddspreg/fp=64)
	 */
	.module arch=mips64r2
	.module fp=64
	.module oddspreg
	.module nomicromips
	.set    mips64r2
	.set    hardfloat
	.set    oddspreg
	.set    noreorder

/*
 * Frame offsets as CPP constants: this gas (2.42 mips) does
 * not fold `.set NAME, value` symbols into instruction
 * immediates -- the symbols survive as relocations and every
 * operand using them rejects. The .S files pass through cpp,
 * so #define folds textually and gas sees literals.
 */
#define OFFS_CALL_REAL_FLAG  0   /* long  */
#define OFFS_REAL_IMPL       8   /* void* */
#define OFFS_RET_VAL         16  /* 8 bytes: int or double bits */
#define OFFS_RET_IS_FP       24  /* int   */
	/* 28..31 padding */
#define OFFS_REAL_A0         32  /* $a0..$a7, 8-byte stride */
#define OFFS_REAL_A1         40
#define OFFS_REAL_A2         48
#define OFFS_REAL_A3         56
#define OFFS_REAL_A4         64
#define OFFS_REAL_A5         72
#define OFFS_REAL_A6         80
#define OFFS_REAL_A7         88
#define OFFS_REAL_F12        96  /* $f12..$f19, 8-byte doubles */
#define OFFS_REAL_F13        104
#define OFFS_REAL_F14        112
#define OFFS_REAL_F15        120
#define OFFS_REAL_F16        128
#define OFFS_REAL_F17        136
#define OFFS_REAL_F18        144
#define OFFS_REAL_F19        152
#define OFFS_REAL_RA         160 /* $31 at entry */
#define OFFS_ORIG_SP         168 /* entry sp: stack args (9+) */
#define FRAME_SIZE           192 /* 16-byte aligned */

	.text

/*
 * WRAPPER_ENTRY_MIPS64 func_name
 *
 * Emits two symbols (the interposed name and the wrapper):
 *   <func_name> / <func_name>_retrace_wrapper
 *
 * 1. Save $a0..$a7, $f12..$f19, $ra, and the entry sp.
 * 2. Zero the control fields.
 * 3. Call retrace_engine_wrapper(name, &frame) -- C ABI.
 * 4. call_real_flag: restore regs, pop the frame, and jr to
 *    real_impl (tail call: its `jr $ra` returns to the caller).
 *    Otherwise load the synthesized return from the frame.
 */
.macro WRAPPER_ENTRY_MIPS64 func_name
	.text
	.global \func_name\()_retrace_wrapper
	.global \func_name
	.type   \func_name\()_retrace_wrapper, @function
	.type   \func_name, @function

\func_name:
\func_name\()_retrace_wrapper:
	.set    noreorder
	/* allocate the frame; 192 keeps sp 16-aligned */
	daddiu  $sp, $sp, -FRAME_SIZE

	/* integer/pointer argument registers */
	sd      $4, OFFS_REAL_A0($sp)
	sd      $5, OFFS_REAL_A1($sp)
	sd      $6, OFFS_REAL_A2($sp)
	sd      $7, OFFS_REAL_A3($sp)
	sd      $8, OFFS_REAL_A4($sp)
	sd      $9, OFFS_REAL_A5($sp)
	sd      $10, OFFS_REAL_A6($sp)
	sd      $11, OFFS_REAL_A7($sp)

	/* FP argument registers (doubles) */
	sdc1    $f12, OFFS_REAL_F12($sp)
	sdc1    $f13, OFFS_REAL_F13($sp)
	sdc1    $f14, OFFS_REAL_F14($sp)
	sdc1    $f15, OFFS_REAL_F15($sp)
	sdc1    $f16, OFFS_REAL_F16($sp)
	sdc1    $f17, OFFS_REAL_F17($sp)
	sdc1    $f18, OFFS_REAL_F18($sp)
	sdc1    $f19, OFFS_REAL_F19($sp)

	/* return address + entry sp */
	sd      $31, OFFS_REAL_RA($sp)
	daddiu  $15, $sp, FRAME_SIZE
	sd      $15, OFFS_ORIG_SP($sp)

	/* zero the control fields */
	sd      $0, OFFS_CALL_REAL_FLAG($sp)
	sd      $0, OFFS_REAL_IMPL($sp)
	sd      $0, OFFS_RET_VAL($sp)
	sw      $0, OFFS_RET_IS_FP($sp)

	/* C call: $4 = name string, $5 = frame.
	 * PC-relative load (the bal trick): %highest/%hi/%lo
	 * against the local .rodata name fail as shared-object
	 * relocations, and the GOT forms need $gp this early-
	 * called wrapper cannot assume. $31 is ALREADY saved in
	 * the frame, so bal may clobber it.
	 */
	.set    noreorder
1:
	bal     2f
	nop
2:
	lui     $4, %hi(\func_name\()_real_name - 2b)
	daddiu  $4, $4, %lo(\func_name\()_real_name - 2b)
	daddu   $4, $4, $31
	daddiu  $5, $sp, 0

	jal     retrace_engine_wrapper
	nop

	/* reload call_real_flag */
	ld      $15, OFFS_CALL_REAL_FLAG($sp)
	beql    $15, $0, 1f
	nop

	/* tail call: restore args, pop, jump to the real impl */
	ld      $25, OFFS_REAL_IMPL($sp)
	ld      $4, OFFS_REAL_A0($sp)
	ld      $5, OFFS_REAL_A1($sp)
	ld      $6, OFFS_REAL_A2($sp)
	ld      $7, OFFS_REAL_A3($sp)
	ld      $8, OFFS_REAL_A4($sp)
	ld      $9, OFFS_REAL_A5($sp)
	ld      $10, OFFS_REAL_A6($sp)
	ld      $11, OFFS_REAL_A7($sp)
	ldc1    $f12, OFFS_REAL_F12($sp)
	ldc1    $f13, OFFS_REAL_F13($sp)
	ldc1    $f14, OFFS_REAL_F14($sp)
	ldc1    $f15, OFFS_REAL_F15($sp)
	ldc1    $f16, OFFS_REAL_F16($sp)
	ldc1    $f17, OFFS_REAL_F17($sp)
	ldc1    $f18, OFFS_REAL_F18($sp)
	ldc1    $f19, OFFS_REAL_F19($sp)
	ld      $31, OFFS_REAL_RA($sp)
	daddiu  $sp, $sp, FRAME_SIZE
	jr      $25
	nop

1:
	/* synthesized return: integer in $v0, double bits in $f0 */
	lw      $15, OFFS_RET_IS_FP($sp)
	beq     $15, $0, 2f
	nop
	ld      $15, OFFS_RET_VAL($sp)
	dmtc1   $15, $f0
	b       3f
	nop
2:
	ld      $2, OFFS_RET_VAL($sp)
3:
	ld      $31, OFFS_REAL_RA($sp)
	daddiu  $sp, $sp, FRAME_SIZE
	jr      $31
	nop
	.set    reorder

	/* the name rides IN .text (aligned): PC-relative %hi/%lo
	 * deltas must not cross sections
	 */
	.align  3
\func_name\()_real_name:
	.asciz  "\func_name"
	.size   \func_name\()_real_name, . - \func_name\()_real_name
	.endm
