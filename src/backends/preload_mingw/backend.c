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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hook.h"
#include "inject.h"

/* The hook table + installation live in win_common/hook_targets.c
 * (shared with MSVC); this file is the spawn/backend surface.
 */

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
	pid = retrace_win_inject_run(cmdline, dll_path, NULL);
	if (pid == 0)
		return RETRACE_BACKEND_INTERNAL;
	return (retrace_pid_t)pid;
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
