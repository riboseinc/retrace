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
 * DLL_PROCESS_ATTACH, walks the prototype registry and installs an inline
 * hook per entry. Refuses-to-hook entries that don't pass the prologue
 * safety check (ADR-0009).
 *
 * This file is shared between preload-msvc and preload-mingw; it contains
 * no compiler-specific code (no #ifdef _MSC_VER). Toolchain differences
 * are handled by separate translation units.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "hook.h"

/*
 * The list of functions to hook is provided by the backend via an
 * externally-defined table. The struct and the table accessor live in
 * win_common/hook.h so both MSVC and MinGW backends share the same
 * definition.
 */

/* Per-hook handles, retained for detach. */
static retrace_hook_t **g_hooks;
static size_t g_hook_count;

static void
install_all_hooks(void)
{
	const struct retrace_hook_target *targets;
	size_t count = 0;
	size_t i;

	targets = retrace_win_hook_targets(&count);
	if (targets == NULL || count == 0)
		return;

	g_hooks = (retrace_hook_t **)
		HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			  count * sizeof(retrace_hook_t *));
	if (g_hooks == NULL)
		return;

	for (i = 0; i < count; i++) {
		retrace_hook_status_t st;
		void *trampoline = NULL;

		if (targets[i].target_addr == NULL || targets[i].wrapper_addr == NULL)
			continue;

		st = retrace_hook_install(targets[i].target_addr,
					  targets[i].wrapper_addr,
					  &trampoline,
					  &g_hooks[g_hook_count]);
		if (st != RETRACE_HOOK_OK) {
			/* Conservative v1: log and skip. */
			OutputDebugStringA("retrace: refused to hook '");
			OutputDebugStringA(targets[i].name ? targets[i].name : "(null)");
			OutputDebugStringA("' (prologue unsafe)\n");
			continue;
		}
		g_hook_count++;
	}
}

static void
uninstall_all_hooks(void)
{
	size_t i;

	if (g_hooks == NULL)
		return;

	for (i = 0; i < g_hook_count; i++) {
		if (g_hooks[i] != NULL)
			retrace_hook_uninstall(g_hooks[i]);
	}

	HeapFree(GetProcessHeap(), 0, g_hooks);
	g_hooks = NULL;
	g_hook_count = 0;
}

BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	(void)hinstDLL;
	(void)lpvReserved;

	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hinstDLL);
#ifdef _MSC_VER
		/* MSVC has no __attribute__((constructor)); MinGW auto-registers
		 * via the constructor attribute in backend.c instead.
		 */
		register_preload_msvc();
#endif
		install_all_hooks();
		break;
	case DLL_PROCESS_DETACH:
		uninstall_all_hooks();
		break;
	default:
		break;
	}

	return TRUE;
}
