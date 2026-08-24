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

static size_t g_last_prologue_len;

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
build_trampoline_x64_ex(void *target, size_t prologue_len,
			const unsigned char *prefix, size_t prefix_len,
			void **out)
{
	/*
	 * Trampoline body:
	 *   <prefix bytes>                         ; thunk arg setup
	 *   <relocated prologue bytes>
	 *   jmp rel32 (target + prologue_len)
	 *
	 * The prefix replays a thunk's argument setup (ucrt thunks
	 * load a hidden variant selector into a register before
	 * tail-jumping the shared worker -- skipping it hands the
	 * worker garbage in that argument).
	 *
	 * The tail MUST be a DIRECT rel32 jump, not mov rax/jmp
	 * rax: the indirect form is a Control Flow Guard check
	 * point, and (target + prologue_len) is mid-function --
	 * not a valid CFG target -- so the process dies with
	 * STATUS_STACK_BUFFER_OVERRUN. The trampoline is
	 * allocated within rel32 range of the target, so the
	 * direct form always fits.
	 */
	const size_t total = prefix_len + prologue_len + 5;
	unsigned char *buf = (unsigned char *)
		retrace_trampoline_alloc_near(target, total);
	ptrdiff_t rel;

	if (buf == NULL)
		return RETRACE_HOOK_NO_MEMORY;

	if (prefix_len > 0)
		memcpy(buf, prefix, prefix_len);
	memcpy(buf + prefix_len, target, prologue_len);

	/*
	 * The tail MUST be a DIRECT rel32 jump, not mov rax/jmp
	 * rax: the indirect form is a Control Flow Guard check
	 * point, and (target + prologue_len) is mid-function --
	 * not a valid CFG target -- so the process dies with
	 * STATUS_STACK_BUFFER_OVERRUN. The trampoline is
	 * allocated within rel32 range of the target, so the
	 * direct form always fits.
	 *
	 * The jump lands PAST the patched window: target[0..len)
	 * holds our jump to the wrapper, so jumping back to
	 * target+0 re-enters the wrapper forever (the loop the
	 * RETRACE_WIN_DIAG counter caught on MSVC's directly
	 * hooked _read). The copied prologue executes here, then
	 * control resumes at target + prologue_len -- the bytes
	 * write_jump never touched.
	 */
	rel = ((const char *)target + (ptrdiff_t)prologue_len) -
	      ((const char *)buf + prefix_len + prologue_len + 5);
	if (rel > 0x7fffffffLL || rel < -0x80000000LL)
		return RETRACE_HOOK_INTERNAL;

	buf[prefix_len + prologue_len] = 0xE9;    /* jmp rel32 */
	memcpy(buf + prefix_len + prologue_len + 1, &rel, 4);

	*out = buf;
	return RETRACE_HOOK_OK;
}

static retrace_hook_status_t
build_trampoline_x64(void *target, size_t prologue_len, void **out)
{
	return build_trampoline_x64_ex(target, prologue_len,
		NULL, 0, out);
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
retrace_hook_install_ex(void *target_addr,
			void *wrapper_addr,
			const unsigned char *prefix,
			size_t prefix_len,
			void **trampoline_out,
			retrace_hook_t **hook_out)
{
	retrace_hook_t *hook;
	size_t patch_size = required_patch();
	size_t prologue_len;
	/* diag evidence (TODO.trace-profile/28): what install ACTUALLY
	 * accepted, per hook -- prologue length + raw target bytes.
	 */
	g_last_prologue_len = 0;
	retrace_hook_status_t st;

	if (target_addr == NULL || wrapper_addr == NULL ||
	    trampoline_out == NULL || hook_out == NULL)
		return RETRACE_HOOK_INTERNAL;
	if (prefix_len > 16)
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

	g_last_prologue_len = prologue_len;

	hook = (retrace_hook_t *)HeapAlloc(GetProcessHeap(), 0, sizeof(*hook));
	if (hook == NULL)
		return RETRACE_HOOK_NO_MEMORY;

	hook->target = target_addr;
	hook->patch_size = prologue_len;
	memcpy(hook->saved_bytes, target_addr, prologue_len);

#ifdef RETRACE_HOOK_ARCH_X64
	st = build_trampoline_x64_ex(target_addr, prologue_len,
		prefix, prefix_len, &hook->trampoline);
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
retrace_hook_install(void *target_addr,
		     void *wrapper_addr,
		     void **trampoline_out,
		     retrace_hook_t **hook_out)
{
	return retrace_hook_install_ex(target_addr, wrapper_addr,
		NULL, 0, trampoline_out, hook_out);
}

retrace_hook_status_t
retrace_hook_bookmark(void *target, size_t patch_size, void *trampoline,
		      retrace_hook_t **hook_out)
{
	retrace_hook_t *hook;

	if (target == NULL || hook_out == NULL || patch_size == 0 ||
	    patch_size > sizeof(hook->saved_bytes))
		return RETRACE_HOOK_INTERNAL;
	hook = (retrace_hook_t *)HeapAlloc(GetProcessHeap(), 0,
		sizeof(*hook));
	if (hook == NULL)
		return RETRACE_HOOK_NO_MEMORY;
	hook->target = target;
	hook->trampoline = trampoline;
	hook->patch_size = patch_size;
	memcpy(hook->saved_bytes, target, patch_size);
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

/* diag evidence (TODO.trace-profile/28): the prologue length the
 * most recent install_ex ACCEPTED (0 = none/refused).
 */
size_t retrace_hook_last_prologue_len(void)
{
	return g_last_prologue_len;
}
