/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Process-creation + DLL injection. See inject.h. Lives in
 * win_common so the backend spawn and the retrace-win-run
 * launcher share ONE implementation.
 */

#include "inject.h"

#include <stddef.h>

DWORD retrace_win_inject_run(const char *cmdline, const char *dll_path,
	DWORD *child_exit_code)
{
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	void *remote_buf = NULL;
	HANDLE remote_thread = NULL;
	HMODULE kernel32;
	typedef FARPROC(WINAPI *load_library_a_t)(LPCSTR);
	load_library_a_t load_library_a;
	DWORD exit_code = 0;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	if (!CreateProcessA(NULL, (LPSTR)cmdline, NULL, NULL, FALSE,
			    CREATE_SUSPENDED, NULL, NULL, &si, &pi))
		return 0;

	remote_buf = VirtualAllocEx(pi.hProcess, NULL,
				    lstrlenA(dll_path) + 1,
				    MEM_COMMIT | MEM_RESERVE,
				    PAGE_READWRITE);
	if (remote_buf == NULL)
		goto fail;

	{
		SIZE_T written = 0;

		if (!WriteProcessMemory(pi.hProcess, remote_buf,
					dll_path,
					lstrlenA(dll_path) + 1, &written))
			goto fail;
	}

	/* LoadLibraryA lives at the same address in both processes
	 * (same bitness, same ASLR base for kernel32).
	 */
	kernel32 = GetModuleHandleA("kernel32.dll");
	if (kernel32 == NULL)
		goto fail;
	load_library_a = (load_library_a_t)GetProcAddress(kernel32,
							  "LoadLibraryA");
	if (load_library_a == NULL)
		goto fail;

	remote_thread = CreateRemoteThread(pi.hProcess, NULL, 0,
		(LPTHREAD_START_ROUTINE)load_library_a,
		remote_buf, 0, NULL);
	if (remote_thread == NULL)
		goto fail;

	/* Wait: DLL_PROCESS_ATTACH installs hooks + boots the engine
	 * inside the child before its main() starts.
	 */
	WaitForSingleObject(remote_thread, INFINITE);
	if (!GetExitCodeThread(remote_thread, &exit_code) || exit_code == 0)
		goto fail;

	CloseHandle(remote_thread);
	VirtualFreeEx(pi.hProcess, remote_buf, 0, MEM_RELEASE);

	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);
	/* Keep the process handle: the caller waits for the child. */
	{
		HANDLE wait = pi.hProcess;

		WaitForSingleObject(wait, INFINITE);
		/*
		 * Forward the child's exit code: launchers exit with
		 * it, so a crashed child is no longer masked by
		 * win-run's own success (TODO.trace-profile/27 round
		 * 5 evidence: exit 0 while the child died silently).
		 */
		if (child_exit_code != NULL)
			GetExitCodeProcess(wait, child_exit_code);
		CloseHandle(wait);
	}
	return pi.dwProcessId;

fail:
	if (remote_thread != NULL)
		CloseHandle(remote_thread);
	if (remote_buf != NULL)
		VirtualFreeEx(pi.hProcess, remote_buf, 0, MEM_RELEASE);
	TerminateProcess(pi.hProcess, 1);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return 0;
}
