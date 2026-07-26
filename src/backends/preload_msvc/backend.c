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
 * preload-msvc backend: native MSVC build of the Windows inline-hooking
 * library.
 *
 * Spawn model (no LD_PRELOAD on Windows):
 *   1. CreateProcess(target_path, CREATE_SUSPENDED)
 *   2. VirtualAllocEx a small buffer in the child for the DLL path
 *   3. CreateRemoteThread(child, LoadLibraryA, dll_path_buffer)
 *   4. WaitForSingleObject on the remote thread (DLL_PROCESS_ATTACH runs
 *      install_all_hooks() inside the child)
 *   5. ResumeThread on the main thread
 *
 * This file is shared verbatim with preload-mingw; the toolchain
 * difference is handled entirely by CMake (different compiler, same
 * sources).
 */

#include <retrace/backend.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "hook.h"

/* ------------------------------------------------------------------ */
/* Hook target table. v1: empty; populated per-function as wrappers */
/* are added. The dllmain walker skips NULL entries. */
/* ------------------------------------------------------------------ */

static const struct retrace_hook_target g_hook_targets[] = {
	/* Example future entry:
	 * { "fopen", NULL, (void *)fopen_retrace_wrapper },
	 *
	 * The target_addr is resolved at install time via GetProcAddress on
	 * the host CRT module (see retrace_win_resolve_targets below).
	 */
	{ NULL, NULL, NULL },  /* sentinel: empty table for v1 */
};

const struct retrace_hook_target *
retrace_win_hook_targets(size_t *count_out)
{
	if (count_out != NULL)
		*count_out = sizeof(g_hook_targets) / sizeof(g_hook_targets[0]);
	return g_hook_targets;
}

/* ------------------------------------------------------------------ */
/* Backend interface. */
/* ------------------------------------------------------------------ */

static int
preload_msvc_probe(struct retrace_engine *eng, const char *target_path)
{
	(void)eng;
	(void)target_path;
#ifdef _MSC_VER
	return 1;
#else
	return 0;
#endif
}

/*
 * Spawn target_path suspended, inject the retrace DLL, resume.
 * Returns the child PID (process ID) on success, or a negative
 * retrace_backend_status_t value on error.
 */
static retrace_pid_t
preload_msvc_spawn(struct retrace_engine *eng,
		    const char *target_path,
		    char *const argv[],
		    char *const envp[])
{
	(void)eng;
	(void)envp;

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	char cmdline[1024];
	const char *dll_path;
	void *remote_buf = NULL;
	HANDLE remote_thread = NULL;
	DWORD exit_code;
	size_t i;
	size_t off;
	HMODULE hKernel32;
	typedef FARPROC(WINAPI *LoadLibraryA_t)(LPCSTR);
	LoadLibraryA_t pLoadLibraryA;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	/* Build the Win32 command line: target_path followed by argv[1..]. */
	off = 0;
	off += lstrlenA(strncpy(cmdline + off, target_path,
				(int)(sizeof(cmdline) - off - 1)));
	for (i = 1; argv != NULL && argv[i] != NULL && off < sizeof(cmdline) - 2; i++) {
		cmdline[off++] = ' ';
		off += lstrlenA(strncpy(cmdline + off, argv[i],
					(int)(sizeof(cmdline) - off - 1)));
	}
	cmdline[off] = '\0';

	/* Resolve DLL path. v1: hard-coded next to the retrace binary; the
	 * engine will eventually pass this through env.
	 */
	dll_path = getenv("RETRACE_V2_LIB");
	if (dll_path == NULL)
		dll_path = "retrace_v2.dll";

	if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
			    CREATE_SUSPENDED, NULL, NULL, &si, &pi))
		return RETRACE_BACKEND_INTERNAL;

	/* Allocate space in the child for the DLL path. */
	remote_buf = VirtualAllocEx(pi.hProcess, NULL,
				    lstrlenA(dll_path) + 1,
				    MEM_COMMIT | MEM_RESERVE,
				    PAGE_READWRITE);
	if (remote_buf == NULL)
		goto fail;

	{
		SIZE_T written = 0;

		if (!WriteProcessMemory(pi.hProcess, remote_buf,
					dll_path, lstrlenA(dll_path) + 1, &written))
			goto fail;
	}

	/* Get LoadLibraryA address (same address in 32-bit/64-bit across
	 * processes when both are the same bitness -- which they are here).
	 */
	hKernel32 = GetModuleHandleA("kernel32.dll");
	if (hKernel32 == NULL)
		goto fail;
	pLoadLibraryA = (LoadLibraryA_t)GetProcAddress(hKernel32, "LoadLibraryA");
	if (pLoadLibraryA == NULL)
		goto fail;

	remote_thread = CreateRemoteThread(pi.hProcess, NULL, 0,
					   (LPTHREAD_START_ROUTINE)pLoadLibraryA,
					   remote_buf, 0, NULL);
	if (remote_thread == NULL)
		goto fail;

	/* Wait for LoadLibrary to finish (DLL_PROCESS_ATTACH runs hooks). */
	WaitForSingleObject(remote_thread, INFINITE);
	if (!GetExitCodeThread(remote_thread, &exit_code) || exit_code == 0)
		goto fail;

	CloseHandle(remote_thread);
	VirtualFreeEx(pi.hProcess, remote_buf, 0, MEM_RELEASE);

	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return (retrace_pid_t)pi.dwProcessId;

fail:
	if (remote_thread != NULL)
		CloseHandle(remote_thread);
	if (remote_buf != NULL)
		VirtualFreeEx(pi.hProcess, remote_buf, 0, MEM_RELEASE);
	TerminateProcess(pi.hProcess, 1);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return RETRACE_BACKEND_INTERNAL;
}

static void
preload_msvc_translate_frame(struct retrace_thread_context *ctx,
			      void *native_frame)
{
	(void)ctx;
	(void)native_frame;
	/* v1: engine integration not yet wired. Wrappers call
	 * retrace_engine_wrapper() directly via the inline-hook trampoline.
	 */
}

static const retrace_backend_t preload_msvc_backend = {
	.name        = "preload-msvc",
	.description = "Native MSVC inline-hook interposition for Windows",
	.rank        = RETRACE_BACKEND_RANK_PREFERRED,
	.probe       = preload_msvc_probe,
	.spawn       = preload_msvc_spawn,
	.attach      = NULL,
	.detach      = NULL,
	.translate_frame = preload_msvc_translate_frame,
};

#ifdef _MSC_VER
/* MSVC has no __attribute__((constructor)); DllMain calls this explicitly
 * (see preload_msvc/dllmain.c). Not static so DllMain can call it.
 */
void register_preload_msvc(void)
{
	retrace_backend_register(&preload_msvc_backend);
}
#else
__attribute__((constructor))
static void
register_preload_msvc(void)
{
	retrace_backend_register(&preload_msvc_backend);
}
#endif
