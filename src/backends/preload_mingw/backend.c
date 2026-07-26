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
 * preload-mingw backend: MSYS2/MinGW build of the Windows inline-hooking
 * library.
 *
 * The hooking core (win_common/) and the spawn logic are identical to
 * preload-msvc; only the constructor attribute and probe() guard differ
 * (gcc instead of _MSC_VER).
 */

#include <retrace/backend.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "hook.h"

static const struct retrace_hook_target g_hook_targets[] = {
	/* Populated per-function as wrappers land. See preload-msvc/backend.c. */
	{ NULL, NULL, NULL },  /* sentinel: empty table for v1 */
};

const struct retrace_hook_target *
retrace_win_hook_targets(size_t *count_out)
{
	if (count_out != NULL)
		*count_out = sizeof(g_hook_targets) / sizeof(g_hook_targets[0]);
	return g_hook_targets;
}

static int
preload_mingw_probe(struct retrace_engine *eng, const char *target_path)
{
	(void)eng;
	(void)target_path;
#if defined(__GNUC__) && defined(_WIN32)
	return 1;
#else
	return 0;
#endif
}

static retrace_pid_t
preload_mingw_spawn(struct retrace_engine *eng,
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

	off = 0;
	off += lstrlenA(strncpy(cmdline + off, target_path,
				(int)(sizeof(cmdline) - off - 1)));
	for (i = 1; argv != NULL && argv[i] != NULL && off < sizeof(cmdline) - 2; i++) {
		cmdline[off++] = ' ';
		off += lstrlenA(strncpy(cmdline + off, argv[i],
					(int)(sizeof(cmdline) - off - 1)));
	}
	cmdline[off] = '\0';

	dll_path = getenv("RETRACE_V2_LIB");
	if (dll_path == NULL)
		dll_path = "retrace_v2.dll";

	if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
			    CREATE_SUSPENDED, NULL, NULL, &si, &pi))
		return RETRACE_BACKEND_INTERNAL;

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
preload_mingw_translate_frame(struct retrace_thread_context *ctx,
			       void *native_frame)
{
	(void)ctx;
	(void)native_frame;
}

static const retrace_backend_t preload_mingw_backend = {
	.name        = "preload-mingw",
	.description = "MSYS2/MinGW inline-hook interposition for Windows",
	.rank        = RETRACE_BACKEND_RANK_FALLBACK,
	.probe       = preload_mingw_probe,
	.spawn       = preload_mingw_spawn,
	.attach      = NULL,
	.detach      = NULL,
	.translate_frame = preload_mingw_translate_frame,
};

#if defined(__GNUC__) && defined(_WIN32)
__attribute__((constructor))
#endif
static void
register_preload_mingw(void)
{
	retrace_backend_register(&preload_mingw_backend);
}
