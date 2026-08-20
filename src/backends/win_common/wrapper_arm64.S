/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Windows arm64 assembly wrappers (TODO.trace-profile/05), GNU-as
 * dialect (MinGW/Clang arm64). The x64 twin is wrapper_x64.S --
 * see there for the rationale; arm64 specifics:
 *
 * AAPCS64: args in x0-x7, then the stack at [entry sp]. No
 * shadow space. The hook entered via a jmp, so x30 still holds
 * the USER's return address and [entry sp] holds stack args --
 * both must survive the wrapper untouched for the tail-jump to
 * work (the real function's epilogue restores x30 from its own
 * frame, which contains the x30 WE leave in the register).
 *
 * Frame (WrapperWinArm64Frame, 112-byte allocation):
 *   [sp+0]   call_real_flag
 *   [sp+8]   real_impl (the hook's trampoline)
 *   [sp+16]  ret_val
 *   [sp+24..+80] x0-x7
 *   [sp+88]  x30 (the user's return address)
 *   [sp+96]  entry sp
 */

	.macro WRAPPER_ENTRY_WIN_ARM64 fname
	.text
	.balign 16
	.globl retrace_wrap_\fname
retrace_wrap_\fname:
	sub	sp, sp, #112

	stp	x0, x1, [sp, #24]
	stp	x2, x3, [sp, #40]
	stp	x4, x5, [sp, #56]
	stp	x6, x7, [sp, #72]
	str	x30, [sp, #88]
	add	x9, sp, #112
	str	x9, [sp, #96]

	stp	xzr, xzr, [sp, #0]
	str	xzr, [sp, #16]

	adr	x0, .Lstr_\fname
	mov	x1, sp
	bl	retrace_engine_wrapper

	ldr	x9, [sp, #0]
	cbnz	x9, .Lcall_\fname

	/* synthesized return: x0 = ret_val, x30 restored */
	ldr	x0, [sp, #16]
	ldr	x30, [sp, #88]
	add	sp, sp, #112
	ret

.Lcall_\fname:
	/* tail-jump the trampoline with the original registers;
	 * the user's return address rides in x30 and [entry sp] */
	ldp	x0, x1, [sp, #24]
	ldp	x2, x3, [sp, #40]
	ldp	x4, x5, [sp, #56]
	ldp	x6, x7, [sp, #72]
	ldr	x30, [sp, #88]
	ldr	x9, [sp, #8]
	add	sp, sp, #112
	br	x9

	.section .rdata
.Lstr_\fname:
	.asciz "\fname"
	.endm

	WRAPPER_ENTRY_WIN_ARM64 fopen
	WRAPPER_ENTRY_WIN_ARM64 open
	WRAPPER_ENTRY_WIN_ARM64 close
	WRAPPER_ENTRY_WIN_ARM64 read
	WRAPPER_ENTRY_WIN_ARM64 write
	WRAPPER_ENTRY_WIN_ARM64 lseek
	WRAPPER_ENTRY_WIN_ARM64 stat
	WRAPPER_ENTRY_WIN_ARM64 unlink
	WRAPPER_ENTRY_WIN_ARM64 remove
	WRAPPER_ENTRY_WIN_ARM64 rename
	WRAPPER_ENTRY_WIN_ARM64 rmdir
	WRAPPER_ENTRY_WIN_ARM64 NtCreateFile
	WRAPPER_ENTRY_WIN_ARM64 NtOpenFile
	WRAPPER_ENTRY_WIN_ARM64 NtQueryAttributesFile
	WRAPPER_ENTRY_WIN_ARM64 NtClose
	WRAPPER_ENTRY_WIN_ARM64 LdrLoadDll
