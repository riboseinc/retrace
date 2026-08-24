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
 * Inline-hook primitives for Windows. From-scratch implementation per
 * ADR-0009: no MinHook, no Detours. BSD-2 license purity.
 *
 * install_hook() overwrites the first N bytes of target_addr with a jump to
 * wrapper_addr, saving the original prologue into an allocated trampoline
 * that the wrapper can call to reach the real function.
 *
 * Two arches:
 *   x64:   14-byte absolute jump (mov rax, imm64; jmp rax), or 5-byte
 *          relative jmp (E9 rel32) when the wrapper is within +/-2GB.
 *   arm64: 16-byte sequence (ldr x16, =addr; br x16) -- always absolute
 *          because arm64 has no 64-bit immediate branch.
 *
 * Conservative v1 policy (see ADR-0009): the first N bytes of the target
 * must be safe-to-relocate. disasm_x64.c / disasm_arm64.c enforce this; if
 * any relative branch or RIP-relative addressing appears in the prologue,
 * install_hook() refuses and returns RETRACE_HOOK_UNSAFE.
 */

#ifndef RETRACE_BACKENDS_WIN_COMMON_HOOK_H
#define RETRACE_BACKENDS_WIN_COMMON_HOOK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle returned by install_hook, freed by uninstall_hook. */
typedef struct retrace_hook retrace_hook_t;

/* Status codes. */
typedef enum {
	RETRACE_HOOK_OK           =  0,
	RETRACE_HOOK_UNSAFE       = -1, /* prologue contains a non-relocatable insn */
	RETRACE_HOOK_TOO_SHORT    = -2, /* prologue shorter than the required patch */
	RETRACE_HOOK_NO_MEMORY    = -3, /* trampoline allocator failed */
	RETRACE_HOOK_PERMISSION   = -4, /* VirtualProtect failed */
	RETRACE_HOOK_INTERNAL     = -99
} retrace_hook_status_t;

/*
 * Install an inline hook. After success, calls to target_addr jump to
 * wrapper_addr; *trampoline_out is a callable pointer that runs the
 * original function (relocated prologue + jump back to the body).
 *
 * The hook is owned by the returned retrace_hook_t handle; pass it to
 * uninstall_hook() to restore the original bytes.
 *
 * Required patch sizes:
 *   x64:   5 bytes (rel32) or 14 bytes (absolute)
 *   arm64: 16 bytes (always)
 */
/*
 * Extended install: `prefix` bytes are replayed in the
 * trampoline BEFORE the relocated prologue -- the argument
 * setup a thunk performed before tail-jumping its worker.
 */
retrace_hook_status_t
retrace_hook_install_ex(void *target_addr,
			void *wrapper_addr,
			const unsigned char *prefix,
			size_t prefix_len,
			void **trampoline_out,
			retrace_hook_t **hook_out);

retrace_hook_status_t
retrace_hook_install(void *target_addr,
		     void *wrapper_addr,
		     void **trampoline_out,
		     retrace_hook_t **hook_out);

/* Restore original bytes and free the hook handle. */
retrace_hook_status_t
retrace_hook_uninstall(retrace_hook_t *hook);

/*
 * Bookmark patch_size bytes at target (captured NOW -- the caller
 * patches afterwards) so a hand-rolled installer can uninstall.
 * retrace_win_install_thunk writes its own 14-byte patch instead
 * of going through retrace_hook_install_ex; without a bookmark the
 * handle stays NULL and uninstall silently skips it, leaving the
 * patch live forever (the post-uninstall fopen loop).
 */
retrace_hook_status_t
retrace_hook_bookmark(void *target, size_t patch_size, void *trampoline,
		      retrace_hook_t **hook_out);

/* Minimum number of prologue bytes that must be safe-to-relocate. */
size_t retrace_hook_required_patch_size(void);

/* Diag evidence (TODO.trace-profile/28): the prologue length the
 * most recent install ACCEPTED (0 = none/refused).
 */
size_t retrace_hook_last_prologue_len(void);

/* Per-process hook-target table descriptor. Each entry maps a libc symbol
 * to the wrapper trampoline that should replace it. The backend (MSVC or
 * MinGW) owns the table; dllmain iterates it to install hooks at startup.
 */
struct retrace_hook_target {
	const char *name;       /* e.g. "fopen" -- for logging only */
	void *target_addr;      /* resolved by caller; NULL to skip */
	void *wrapper_addr;     /* wrapper trampoline */
};

/* Returns a pointer to the first target and writes the table length to
 * *count_out. Implemented by the active backend (preload_msvc/backend.c or
 * preload_mingw/backend.c).
 */
const struct retrace_hook_target *retrace_win_hook_targets(size_t *count_out);

/* MSVC has no __attribute__((constructor)); DllMain calls this to register
 * the active Windows backend with the registry. Implemented by
 * preload_msvc/backend.c.
 */
void register_preload_msvc(void);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_BACKENDS_WIN_COMMON_HOOK_H */
