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
#include "inject.h"

/* The hook table + installation live in win_common/hook_targets.c
 * (shared with MinGW); this file is the spawn/backend surface.
 */

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
	char cmdline[1024];
	const char *dll_path;
	size_t i;
	size_t off;
	DWORD pid;

	(void)eng;
	(void)envp;

	/* Build the Win32 command line: target then argv[1..]. */
	off = (size_t)snprintf(cmdline, sizeof(cmdline), "\"%s\"",
		target_path);
	for (i = 1; argv != NULL && argv[i] != NULL &&
	     off < sizeof(cmdline) - 2; i++)
		off += (size_t)snprintf(cmdline + off,
			sizeof(cmdline) - off, " %s", argv[i]);

	dll_path = getenv("RETRACE_V2_LIB");
	if (dll_path == NULL)
		dll_path = "retrace.dll";

	/*
	 * The injection dance (shared with retrace-win-run):
	 * suspend-create -> inject -> hooks+boot inside the child
	 * -> resume.
	 */
	pid = retrace_win_inject_run(cmdline, dll_path);
	if (pid == 0)
		return RETRACE_BACKEND_INTERNAL;
	return (retrace_pid_t)pid;
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
