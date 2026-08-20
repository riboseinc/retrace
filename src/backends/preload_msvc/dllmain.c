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
 * DLL entry point for the Windows inline-hooking backends. On
 * DLL_PROCESS_ATTACH: install the hook set, then boot the
 * engine (registries, config, real-impls -- MSVC has no
 * constructors, so this is the boot point; see the ordering
 * note at the install call). Refused hooks (unsafe prologue,
 * missing export) are logged and skipped (ADR-0009).
 *
 * This file is shared between preload-msvc and preload-mingw; it contains
 * no compiler-specific code (no #ifdef _MSC_VER). Toolchain differences
 * are handled by separate translation units.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "hook_targets.h"

/*
 * The list of functions to hook is provided by the backend via an
 * externally-defined table. The struct and the table accessor live in
 * win_common/hook.h so both MSVC and MinGW backends share the same
 * definition.
 */

/* The engine boots BEFORE any hook exists: a hooked call must
 * find initialized registries, config, and real-impls.
 * retrace_core_boot() is the MSVC constructor equivalent
 * (src/core/main.c). Hook installation/refusal lives in
 * hook_targets.c (the table, resolution, trampoline map).
 */
void retrace_core_boot(void);

BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	(void)hinstDLL;
	(void)lpvReserved;

	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hinstDLL);
		/*
		 * Hooks FIRST, boot second: real-impl resolution runs
		 * during boot and a hooked name must resolve to its
		 * TRAMPOLINE (resolving after boot would capture the
		 * patched bytes and re-enter the wrapper forever).
		 * Calls racing the boot pass through the wrapper's
		 * !retrace_inited path straight to the trampoline.
		 */
		retrace_win_install_hooks();
		retrace_core_boot();
		break;
	case DLL_PROCESS_DETACH:
		retrace_win_uninstall_hooks();
		break;
	default:
		break;
	}

	return TRUE;
}
