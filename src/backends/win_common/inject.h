/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_BACKENDS_WIN_COMMON_INJECT_H_
#define RETRACE_BACKENDS_WIN_COMMON_INJECT_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Process-creation + DLL injection (TODO.windows/05). Shared by
 * the backend's spawn (inside the library) and the retrace-win-run
 * launcher -- ONE implementation of the dance:
 *
 *   CreateProcess(CREATE_SUSPENDED)
 *   -> VirtualAllocEx + WriteProcessMemory (DLL path)
 *   -> CreateRemoteThread(LoadLibraryA)
 *   -> wait (DLL_PROCESS_ATTACH: hooks + engine boot in child)
 *   -> ResumeThread
 *
 * Returns the child PID (> 0) on success, 0 on failure.
 */
/*
 * Launch cmdline suspended, inject dll_path, wait for the child.
 * Returns the child pid (0 = launch/inject failed). When
 * child_exit_code is non-NULL it receives the child's exit code
 * (launchers forward it; a crashed child must not be masked).
 */
DWORD retrace_win_inject_run(const char *cmdline, const char *dll_path,
	DWORD *child_exit_code);

/*
 * The non-blocking split (TODO.impl/11): launch suspended,
 * inject, resume -- and RETURN without waiting. The daemon's
 * spawn seam uses this so the poll loop can reap the child
 * later (GetExitCodeProcess is SIGCHLD's Windows analogue).
 * `env_block` is a Windows environment-strings blob
 * ("NAME=VALUE\0...\0\0") or NULL to inherit. On success
 * returns the pid and, when child_out is non-NULL, leaves the
 * PROCESS handle for the caller (it closes it after reaping).
 */
DWORD retrace_win_inject_spawn(const char *cmdline,
	const char *dll_path, const char *env_block,
	HANDLE *child_out);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_BACKENDS_WIN_COMMON_INJECT_H_ */
