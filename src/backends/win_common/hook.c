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
 * Inline-hook installer. See hook.h.
 *
 * Layout of an installed hook:
 *
 *   target_addr:   [ original N bytes overwritten with a jump to wrapper ]
 *   trampoline:    [ relocated N bytes (copy of original prologue) ]
 *                  [ jump back to target_addr + N ]
 *
 * The wrapper receives control with the original arguments intact; it can
 * call *trampoline_out to invoke the real function.
 *
 * Patch sizes:
 *   x64:   5 (rel32 jmp) if the wrapper is within +/-2GB; otherwise 14
 *          (mov rax, imm64; jmp rax).
 *   arm64: 16 (ldr x16, =addr; br x16).
 */

#include "hook.h"
#include "trampoline_allocator.h"
#include "disasm.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if defined(_M_ARM64) || defined(__aarch64__)
#  define RETRACE_HOOK_ARCH_ARM64 1
#else
#  define RETRACE_HOOK_ARCH_X64 1
#endif

struct retrace_hook {
	void *target;
	void *trampoline;
	size_t patch_size;
	unsigned char saved_bytes[16]; /* original bytes, for uninstall */
};

/* x64: +/-2GB window for rel32 jmp. */
#define RETRACE_REL32_WINDOW ((ptrdiff_t)(1L << 30))

static int
within_rel32(const void *from, const void *to)
{
	ptrdiff_t delta = (const char *)to - (const char *)from;
	/* The jump instruction is 5 bytes; the rel32 is measured from the
	 * end of that instruction, so adjust.
	 */
	delta -= 5;
	return (delta >= -RETRACE_REL32_WINDOW && delta <= RETRACE_REL32_WINDOW);
}

static size_t
required_patch(void)
{
#ifdef RETRACE_HOOK_ARCH_ARM64
	return 16;
#else
	return 14; /* default to absolute on x64; rel32 is chosen per-call */
#endif
}

size_t
retrace_hook_required_patch_size(void)
{
	return required_patch();
}

/* --- x64 implementation --- */

#ifdef RETRACE_HOOK_ARCH_X64

static retrace_hook_status_t
build_trampoline_x64(void *target, size_t prologue_len, void **out)
{
	/* Trampoline body:
	 *   <relocated prologue bytes>
	 *   mov rax, (target + prologue_len)     ; 48 B8 + 8 bytes
	 *   jmp rax                                ; FF E0
	 * Total: prologue_len + 10 + 2 bytes.
	 */
	const size_t total = prologue_len + 12;
	unsigned char *buf = (unsigned char *)
		retrace_trampoline_alloc_near(target, total);
	ULONG_PTR back_addr;

	if (buf == NULL)
		return RETRACE_HOOK_NO_MEMORY;

	memcpy(buf, target, prologue_len);

	back_addr = (ULONG_PTR)target + prologue_len;
	buf[prologue_len + 0] = 0x48;            /* REX.W */
	buf[prologue_len + 1] = 0xB8;            /* mov rax, imm64 */
	memcpy(buf + prologue_len + 2, &back_addr, 8);
	buf[prologue_len + 10] = 0xFF;           /* jmp r/m64 */
	buf[prologue_len + 11] = 0xE0;           /* ModR/M: r/m=rax */

	*out = buf;
	return RETRACE_HOOK_OK;
}

static retrace_hook_status_t
write_jump_x64(void *target, void *wrapper, int use_rel32, size_t patch_size)
{
	unsigned char patch[32];
	DWORD old_protect = 0;
	size_t jump_len;
	size_t i;

	if (patch_size > sizeof(patch))
		return RETRACE_HOOK_INTERNAL;

	if (use_rel32) {
		/* E9 rel32 */
		ptrdiff_t rel = (const char *)wrapper - ((const char *)target + 5);

		patch[0] = 0xE9;
		memcpy(patch + 1, &rel, 4);
		jump_len = 5;
	} else {
		/* mov rax, imm64; jmp rax (12 bytes), pad to patch_size with nops. */
		patch[0] = 0x48;
		patch[1] = 0xB8;
		memcpy(patch + 2, &wrapper, 8);
		patch[10] = 0xFF;
		patch[11] = 0xE0;
		jump_len = 12;
	}

	if (jump_len > patch_size)
		return RETRACE_HOOK_INTERNAL;

	/* Pad the remainder with nops so the whole [0, patch_size) window is
	 * valid instructions and the trampoline's copy of the original
	 * prologue lines up exactly with the bytes we overwrote.
	 */
	for (i = jump_len; i < patch_size; i++)
		patch[i] = 0x90;

	if (!VirtualProtect(target, patch_size, PAGE_EXECUTE_READWRITE, &old_protect))
		return RETRACE_HOOK_PERMISSION;

	memcpy(target, patch, patch_size);

	/* Restore the original protection (the page is now executable and
	 * holds our patch; flushing the icache is the OS's job on x86/x64).
	 */
	{
		DWORD tmp;

		VirtualProtect(target, patch_size, old_protect, &tmp);
	}
	FlushInstructionCache(GetCurrentProcess(), target, patch_size);
	return RETRACE_HOOK_OK;
}

#endif /* RETRACE_HOOK_ARCH_X64 */

/* --- arm64 implementation --- */

#ifdef RETRACE_HOOK_ARCH_ARM64

static retrace_hook_status_t
build_trampoline_arm64(void *target, size_t prologue_len, void **out)
{
	/* Trampoline body:
	 *   <relocated prologue (4 bytes each)>
	 *   ldr x16, =back_addr    ; encoded as LDR literal: 0x58000050
	 *                            followed by 8-byte literal
	 *   br  x16                 ; D61F0200
	 * Total: prologue_len + 4 + 8 + 4 = prologue_len + 16 bytes.
	 *
	 * Simpler & deterministic: use the same 16-byte absolute-jump trick
	 * as the patch:
	 *   ldr x16, [pc, #8]      ; load the next 8 bytes
	 *   br  x16
	 *   .quad back_addr
	 */
	const size_t total = prologue_len + 16;
	unsigned char *buf = (unsigned char *)
		retrace_trampoline_alloc_near(target, total);
	ULONG_PTR back_addr;
	/* ldr x16, [pc, #8] -> 0x58000050 little-endian = 50 00 00 58
	 * br x16            -> 0xD61F0200 little-endian = 00 02 1F D6
	 */
	static const unsigned char ldr_x16_pc8[4] = { 0x50, 0x00, 0x00, 0x58 };
	static const unsigned char br_x16[4]      = { 0x00, 0x02, 0x1F, 0xD6 };

	if (buf == NULL)
		return RETRACE_HOOK_NO_MEMORY;

	memcpy(buf, target, prologue_len);

	memcpy(buf + prologue_len + 0, ldr_x16_pc8, 4);
	memcpy(buf + prologue_len + 4, br_x16, 4);
	back_addr = (ULONG_PTR)target + prologue_len;
	memcpy(buf + prologue_len + 8, &back_addr, 8);

	*out = buf;
	return RETRACE_HOOK_OK;
}

static retrace_hook_status_t
write_jump_arm64(void *target, void *wrapper)
{
	/* Patch: ldr x16, [pc, #8]; br x16; .quad wrapper_addr. 16 bytes. */
	unsigned char patch[16];
	DWORD old_protect = 0;
	static const unsigned char ldr_x16_pc8[4] = { 0x50, 0x00, 0x00, 0x58 };
	static const unsigned char br_x16[4]      = { 0x00, 0x02, 0x1F, 0xD6 };

	memcpy(patch + 0, ldr_x16_pc8, 4);
	memcpy(patch + 4, br_x16, 4);
	memcpy(patch + 8, &wrapper, 8);

	if (!VirtualProtect(target, 16, PAGE_EXECUTE_READWRITE, &old_protect))
		return RETRACE_HOOK_PERMISSION;

	memcpy(target, patch, 16);
	{
		DWORD tmp;

		VirtualProtect(target, 16, old_protect, &tmp);
	}
	FlushInstructionCache(GetCurrentProcess(), target, 16);
	return RETRACE_HOOK_OK;
}

#endif /* RETRACE_HOOK_ARCH_ARM64 */

retrace_hook_status_t
retrace_hook_install(void *target_addr,
		     void *wrapper_addr,
		     void **trampoline_out,
		     retrace_hook_t **hook_out)
{
	retrace_hook_t *hook;
	size_t patch_size = required_patch();
	size_t prologue_len;
	retrace_hook_status_t st;

	if (target_addr == NULL || wrapper_addr == NULL ||
	    trampoline_out == NULL || hook_out == NULL)
		return RETRACE_HOOK_INTERNAL;

#ifdef RETRACE_HOOK_ARCH_X64
	{
		int use_rel32 = within_rel32(target_addr, wrapper_addr);
		/* If using rel32, the patch is only 5 bytes. We still need to
		 * decode enough prologue to cover the 5 bytes we overwrite.
		 */
		size_t need = use_rel32 ? 5 : patch_size;

		prologue_len = retrace_disasm_x64_prologue_len(
			(const unsigned char *)target_addr, need, 32);
		if (prologue_len == 0)
			return RETRACE_HOOK_UNSAFE;
	}
#else
	{
		prologue_len = retrace_disasm_arm64_prologue_len(
			(const unsigned char *)target_addr, patch_size, 32);
		if (prologue_len == 0)
			return RETRACE_HOOK_UNSAFE;
	}
#endif

	hook = (retrace_hook_t *)HeapAlloc(GetProcessHeap(), 0, sizeof(*hook));
	if (hook == NULL)
		return RETRACE_HOOK_NO_MEMORY;

	hook->target = target_addr;
	hook->patch_size = prologue_len;
	memcpy(hook->saved_bytes, target_addr, prologue_len);

#ifdef RETRACE_HOOK_ARCH_X64
	st = build_trampoline_x64(target_addr, prologue_len, &hook->trampoline);
#else
	st = build_trampoline_arm64(target_addr, prologue_len, &hook->trampoline);
#endif
	if (st != RETRACE_HOOK_OK) {
		HeapFree(GetProcessHeap(), 0, hook);
		return st;
	}

#ifdef RETRACE_HOOK_ARCH_X64
	{
		int use_rel32 = within_rel32(target_addr, wrapper_addr);

		st = write_jump_x64(target_addr, wrapper_addr, use_rel32, prologue_len);
	}
#else
	st = write_jump_arm64(target_addr, wrapper_addr);
#endif
	if (st != RETRACE_HOOK_OK) {
		retrace_trampoline_free(hook->trampoline);
		HeapFree(GetProcessHeap(), 0, hook);
		return st;
	}

	*trampoline_out = hook->trampoline;
	*hook_out = hook;
	return RETRACE_HOOK_OK;
}

retrace_hook_status_t
retrace_hook_uninstall(retrace_hook_t *hook)
{
	DWORD old_protect = 0;
	size_t len;

	if (hook == NULL)
		return RETRACE_HOOK_INTERNAL;

	len = hook->patch_size;
	if (!VirtualProtect(hook->target, len, PAGE_EXECUTE_READWRITE, &old_protect))
		return RETRACE_HOOK_PERMISSION;

	memcpy(hook->target, hook->saved_bytes, len);
	{
		DWORD tmp;

		VirtualProtect(hook->target, len, old_protect, &tmp);
	}
	FlushInstructionCache(GetCurrentProcess(), hook->target, len);

	retrace_trampoline_free(hook->trampoline);
	HeapFree(GetProcessHeap(), 0, hook);
	return RETRACE_HOOK_OK;
}
