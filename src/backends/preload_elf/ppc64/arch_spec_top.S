/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Per-arch trampoline core for PPC64 little-endian, ELFv2
 * (TODO.impl/20).
 *
 * The frame layout MUST stay in sync with struct WrapperPpc64Frame
 * in arch_spec_bottom.c.
 *
 * PPC64 ELFv2 ABI recap:
 *   - Integer/pointer args: r3..r10
 *   - FP args (doubles):    f1..f8 (independent sequence, no
 *     mixed GPR/FPR counting -- that was ELFv1)
 *   - Return address in LR; integer return r3, FP return f1
 *   - r2 (TOC): valid at entry with OUR module's TOC (the
 *     interposition call sequence set it); the engine is hidden
 *     so `bl` is a LOCAL call and r2 stays untouched. The frame
 *     deliberately does not save r2.
 *   - stdu builds the back-chain word (the entry sp) at [sp]
 *     automatically -- offset 0 doubles as OFFS_ORIG_SP.
 *   - Stack args (9+) live in the caller's parameter save area,
 *     which starts at caller_sp+32 in ELFv2.
 *   - No delay slots. Entries align to 4 bytes.
 */

/*
 * Frame offsets as CPP constants: this file AND funcs_symbols.S
 * pass through cpp together (the .include-vs-#include lesson
 * from the MIPS port), so #define folds textually.
 */
#define OFFS_ORIG_SP         0   /* back chain: sp at entry */
#define OFFS_ENTRY_GLOBAL    8   /* int: 1 = global entry (restore r2) */
	/* 8/16/24 are the ELFv2 per-frame reserved slots
	 * (CR save, LR save, TOC save). SP+16 is the CALLEE's
	 * LR-save slot -- a C function's prologue writes
	 * `std r0, 16(r1)` BEFORE its own stdu, overwriting
	 * anything we stored there. The original caller's LR
	 * lives at OFFS_SAVED_LR past the ABI-volatile zone.
	 * SP+24 holds the caller's TOC across the engine call.
	 *
	 * Native root cause (TODO.impl/20 r19): LR at SP+16 made
	 * the engine prologue replace it with the trampoline's
	 * after-bl address; the tail mtlr bounced real-malloc's
	 * return into the middle of the trampoline, double-popped
	 * the frame, and bctr'd into stack garbage.
	 */
#define OFFS_CALL_REAL_FLAG  32  /* long  */
#define OFFS_REAL_IMPL       40  /* void* */
#define OFFS_RET_VAL         48  /* 8 bytes: int or double bits */
#define OFFS_RET_IS_FP       56  /* int   */
	/* 60..63 padding */
#define OFFS_REAL_R3         64  /* r3..r10, 8-byte stride */
#define OFFS_REAL_R4         72
#define OFFS_REAL_R5         80
#define OFFS_REAL_R6         88
#define OFFS_REAL_R7         96
#define OFFS_REAL_R8         104
#define OFFS_REAL_R9         112
#define OFFS_REAL_R10        120
#define OFFS_REAL_F1         128 /* f1..f8, 8-byte doubles */
#define OFFS_REAL_F2         136
#define OFFS_REAL_F3         144
#define OFFS_REAL_F4         152
#define OFFS_REAL_F5         160
#define OFFS_REAL_F6         168
#define OFFS_REAL_F7         176
#define OFFS_REAL_F8         184
	/* private: original caller LR -- past ABI zone SP+0..95 */
#define OFFS_SAVED_LR        192
#define FRAME_SIZE           208 /* 16-byte aligned */

/*
 * WRAPPER_ENTRY_PPC64 func_name
 *
 * Emits two symbols (the interposed name and the wrapper):
 *   <func_name> / <func_name>_retrace_wrapper
 */
.macro WRAPPER_ENTRY_PPC64 func_name
	.text
	/* ELFv2: function entries are 16-byte aligned and every
	 * trampoline advertises a local entry (.localentry). With
	 * only 4-byte alignment half the entries sit 8-mod-16 and
	 * the advertised local entries land mid-instruction for
	 * same-TOC callers -- the loader binds calls to garbage
	 * mid-wrapper offsets (the native-gdb ghost: dispatch
	 * through unwritten frame slots, pc = a stack address).
	 * The MIPS odd-dynsym lesson, ppc64 flavor: .align 4.
	 */
	.align  4
	.globl  \func_name\()_retrace_wrapper
	.globl  \func_name
	.type   \func_name\()_retrace_wrapper, @function
	.type   \func_name, @function

\func_name:
\func_name\()_retrace_wrapper:
	/* Global entry (ELFv2): the caller set r12 = this entry's
	 * address and r2 = the CALLER's TOC; establish OUR TOC
	 * before anything touches it. Local callers (our own `bl`
	 * to the engine, which is a hidden same-module symbol)
	 * branch to the local entry below and skip this. Without
	 * it the name-string TOC addressing deref'd the caller's
	 * TOC -- the first-interposition SIGSEGV.
	 */
	/* the .TOC.-entry delta is what ld's global-entry magic
	 * keys on -- a bare .TOC.@ha emits a static ADDR16 pair
	 * the loader rejects (the 'out of range' failure) */
	/* save the INCOMING r2 (the caller's TOC) before the
	 * pair overwrites it. A nop pads the global entry to 16
	 * bytes: .localentry demands a power-of-two offset. Both
	 * internal callers (direct-bound to the local entry,
	 * no PLT stub, nothing valid at their slot 24) and
	 * external ones need this -- the tail call hands the
	 * caller's TOC to the real impl, because resolved IFUNC
	 * variants are internal functions with no global entry
	 * that run with whatever r2 they are given.
	 */
	mr      11, 2
	addis   2, 12, .TOC.-\func_name@ha
	addi    2, 2, .TOC.-\func_name@l
	b       .Lglob_\func_name
	.localentry \func_name, .-\func_name
	.localentry \func_name\()_retrace_wrapper, \
		.-\func_name\()_retrace_wrapper

/* LOCAL ENTRY (+16): our own .so's internal calls bind here
 * (same-TOC, r2 already ours, r11/r12 garbage). The caller's
 * r2 must be returned untouched -- mark the frame so the tail
 * skips the TOC restore. Slot 8 (the ABI CR-save slot, unused)
 * carries the entry flag.
 */
.Llocal_\func_name:
.Llocal_\func_name\()_retrace_wrapper:
	li      0, 0
	b       .Lframe_\func_name
.Lglob_\func_name:
	li      0, 1
.Lframe_\func_name:
	/* allocate the frame; stdu stores the entry sp at [r1]
	 * (the back chain), which IS OFFS_ORIG_SP */
	stdu    1, -FRAME_SIZE(1)
	stw     0, OFFS_ENTRY_GLOBAL(1)
	/* 24 is the ELFv2 TOC-save slot: stash the caller's r2
	 * (global entries only; the tail consults the flag) */
	std     11, 24(1)

	/* integer/pointer argument registers r3..r10 */
	std     3, OFFS_REAL_R3(1)
	std     4, OFFS_REAL_R4(1)
	std     5, OFFS_REAL_R5(1)
	std     6, OFFS_REAL_R6(1)
	std     7, OFFS_REAL_R7(1)
	std     8, OFFS_REAL_R8(1)
	std     9, OFFS_REAL_R9(1)
	std     10, OFFS_REAL_R10(1)

	/* FP argument registers f1..f8 (doubles) */
	stfd    1, OFFS_REAL_F1(1)
	stfd    2, OFFS_REAL_F2(1)
	stfd    3, OFFS_REAL_F3(1)
	stfd    4, OFFS_REAL_F4(1)
	stfd    5, OFFS_REAL_F5(1)
	stfd    6, OFFS_REAL_F6(1)
	stfd    7, OFFS_REAL_F7(1)
	stfd    8, OFFS_REAL_F8(1)

	/* link register */
	mflr    0
	std     0, OFFS_SAVED_LR(1)

	/* zero the control fields. PPC has NO hardwired-zero
	 * register (unlike MIPS $0 / RISC-V x0 / AArch64 xzr):
	 * r0 already holds the saved LR here, so the stores must
	 * use a genuinely-zeroed volatile temp -- zeroing via r0
	 * initialized call_real_flag/real_impl/ret_val/ret_is_fp
	 * with the LR pointer and jumped the tail call through
	 * garbage (the init-time SIGSEGV).
	 */
	li      8, 0
	std     8, OFFS_CALL_REAL_FLAG(1)
	std     8, OFFS_REAL_IMPL(1)
	std     8, OFFS_RET_VAL(1)
	stw     8, OFFS_RET_IS_FP(1)

	/* C call: r3 = name string (TOC-relative), r4 = frame.
	 * r2 is ours since the global-entry prologue. The engine
	 * is hidden, so `bl` resolves to its LOCAL entry: r2 stays
	 * ours and the callee skips its own r2 setup. LR is ALREADY
	 * saved in the frame, so the call may clobber it.
	 */
	addis   3, 2, \func_name\()_real_name@got@ha
	ld      3, \func_name\()_real_name@got@l(3)
	addi    4, 1, 0

	bl      retrace_engine_wrapper
	nop

	/* reload call_real_flag */
	ld      8, OFFS_CALL_REAL_FLAG(1)
	cmpdi   8, 0
	bne     .Lcall_real_\func_name

	/* synthesized return: integer in r3, double bits in f1 */
	lwz     8, OFFS_RET_IS_FP(1)
	cmpwi   8, 0
	bne     .Lret_fp_\func_name
	ld      3, OFFS_RET_VAL(1)
	b       .Lexit_\func_name
.Lret_fp_\func_name:
	lfd     1, OFFS_RET_VAL(1)
.Lexit_\func_name:
	lwz     8, OFFS_ENTRY_GLOBAL(1)
	cmpwi   8, 0
	beq     .Lnotoc_\func_name
	ld      2, 24(1)
.Lnotoc_\func_name:
	ld      0, OFFS_SAVED_LR(1)
	mtlr    0
	addi    1, 1, FRAME_SIZE
	blr

.Lcall_real_\func_name:
	/* tail call: restore args, pop, jump to the real impl via
	 * ctr; its `blr` returns straight to the original caller
	 * (LR restored below) */
	ld      12, OFFS_REAL_IMPL(1)
	ld      3, OFFS_REAL_R3(1)
	ld      4, OFFS_REAL_R4(1)
	ld      5, OFFS_REAL_R5(1)
	ld      6, OFFS_REAL_R6(1)
	ld      7, OFFS_REAL_R7(1)
	ld      8, OFFS_REAL_R8(1)
	ld      9, OFFS_REAL_R9(1)
	ld      10, OFFS_REAL_R10(1)
	lfd     1, OFFS_REAL_F1(1)
	lfd     2, OFFS_REAL_F2(1)
	lfd     3, OFFS_REAL_F3(1)
	lfd     4, OFFS_REAL_F4(1)
	lfd     5, OFFS_REAL_F5(1)
	lfd     6, OFFS_REAL_F6(1)
	lfd     7, OFFS_REAL_F7(1)
	lfd     8, OFFS_REAL_F8(1)
	ld      0, OFFS_SAVED_LR(1)
	mtlr    0
	/* give the real impl the caller's TOC -- global entries
	 * only; a local-entry caller's TOC is already ours */
	lwz     0, OFFS_ENTRY_GLOBAL(1)
	cmpwi   0, 0
	beq     .Lnotoc2_\func_name
	ld      2, 24(1)
.Lnotoc2_\func_name:
	addi    1, 1, FRAME_SIZE
	mtctr   12
	bctr

	/* the name lives in .rodata: shared, never written */
	.section .rodata
\func_name\()_real_name:
	.asciz  "\func_name"
	.size   \func_name\()_real_name, . - \func_name\()_real_name
	.endm
