/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Windows x64 assembly wrappers (TODO.windows/05), GNU-as
 * dialect (MinGW). wrapper_x64.asm is the MASM twin -- identical
 * semantics.
 *
 * One wrapper per hooked function. On entry the inline hook has
 * jumped here with the ORIGINAL Microsoft-x64 register state:
 * rcx/rdx/r8/r9 hold args 0..3, [rsp] is the USER's return
 * address, args 5+ live at [rsp+0x28..].
 *
 * Stack layout for the engine call (120 bytes, entry rsp % 16 ==
 * 8, so after `sub 120` rsp is 16-aligned at the call site):
 *
 *   [rsp .. rsp+31]    shadow space for retrace_engine_wrapper
 *   [rsp+32 .. +111]   struct WrapperWinX64Frame (80 bytes)
 *                       +0  call_real_flag
 *                       +8  real_impl (the hook's trampoline)
 *                       +16 ret_val
 *                       +24 r9   +32 r8   +40 rcx
 *                       +48 rdx  +56 rsi  +64 rdi
 *                       +72 entry rsp
 *
 * The shadow space MUST NOT overlap the frame: the C engine can
 * spill rcx/rdx home slots into it, which would clobber
 * real_impl and the saved registers.
 *
 * After the engine returns: flag == 1 -> restore the original
 * argument registers, load the trampoline into r11, drop the
 * frame and TAIL-JUMP -- the trampoline executes the relocated
 * prologue, jumps back into the body at target+N, and the real
 * function's ret returns straight to the USER (Detours shape;
 * the user's return address is still on top of the stack).
 * flag == 0 -> rax = synthesized ret_val, ret to the user.
 */

	.macro WRAPPER_ENTRY_WIN_X64 fname, idx
	.text
	.balign 16
	.globl retrace_wrap_\fname
retrace_wrap_\fname:
	subq	$120, %rsp

	movq	%r9,  56(%rsp)		/* frame+24 */
	movq	%r8,  64(%rsp)		/* frame+32 */
	movq	%rcx, 72(%rsp)		/* frame+40 */
	movq	%rdx, 80(%rsp)		/* frame+48 */
	movq	%rsi, 88(%rsp)		/* frame+56 */
	movq	%rdi, 96(%rsp)		/* frame+64 */
	leaq	120(%rsp), %rax
	movq	%rax, 104(%rsp)		/* frame+72: entry rsp */

	movq	$0, 32(%rsp)		/* frame+0: call_real_flag */
	movq	$0, 40(%rsp)		/* frame+8: real_impl */
	movq	$0, 48(%rsp)		/* frame+16: ret_val */

	/*
	 * Label-free: the name lives in the C-side table
	 * (hook_targets.c), same order as the entries below.
	 */
	movl	\idx, %ecx
	leaq	32(%rsp), %rdx
	call	retrace_win_enter

	cmpq	$1, 32(%rsp)
	jne	.Lsynth_\fname

	movq	56(%rsp), %r9
	movq	64(%rsp), %r8
	movq	72(%rsp), %rcx
	movq	80(%rsp), %rdx
	movq	40(%rsp), %r11
	addq	$120, %rsp
	jmp	*%r11

.Lsynth_\fname:
	movq	48(%rsp), %rax
	addq	$120, %rsp
	ret

	.endm

	WRAPPER_ENTRY_WIN_X64 fopen, 0
	WRAPPER_ENTRY_WIN_X64 open, 6
	WRAPPER_ENTRY_WIN_X64 close, 7
	WRAPPER_ENTRY_WIN_X64 read, 8
	WRAPPER_ENTRY_WIN_X64 write, 9
	WRAPPER_ENTRY_WIN_X64 lseek, 10
	WRAPPER_ENTRY_WIN_X64 stat, 11
	WRAPPER_ENTRY_WIN_X64 unlink, 12
	WRAPPER_ENTRY_WIN_X64 remove, 13
	WRAPPER_ENTRY_WIN_X64 rename, 14
	WRAPPER_ENTRY_WIN_X64 rmdir, 15
	WRAPPER_ENTRY_WIN_X64 NtCreateFile, 1
	WRAPPER_ENTRY_WIN_X64 NtOpenFile, 2
	WRAPPER_ENTRY_WIN_X64 NtQueryAttributesFile, 3
	WRAPPER_ENTRY_WIN_X64 NtClose, 4
	WRAPPER_ENTRY_WIN_X64 LdrLoadDll, 5
	WRAPPER_ENTRY_WIN_X64 getenv, 16
	WRAPPER_ENTRY_WIN_X64 connect, 17
	WRAPPER_ENTRY_WIN_X64 send, 18
	WRAPPER_ENTRY_WIN_X64 recv, 19
	WRAPPER_ENTRY_WIN_X64 NtWriteFile, 20
	WRAPPER_ENTRY_WIN_X64 NtReadFile, 21
	WRAPPER_ENTRY_WIN_X64 NtQueryDirectoryFile, 22
