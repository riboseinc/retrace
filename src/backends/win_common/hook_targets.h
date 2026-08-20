/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_BACKENDS_WIN_COMMON_HOOK_TARGETS_H_
#define RETRACE_BACKENDS_WIN_COMMON_HOOK_TARGETS_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The Windows hook set (TODO.windows/05-06).
 *
 * Layer choice (ADR-0009): the ucrt layer is the default --
 * equivalent to POSIX's "interpose the C library". The ntdll
 * layer is STRICTLY OPT-IN: apps that bypass the CRT (libsass's
 * importer calls CreateFileW/GetFileAttributesW directly) are
 * only visible one layer deeper, but hooking ntdll is what
 * AV/EDR products watch, so it never happens unless the user
 * asks for it via RETRACE_WIN_NTDLL=1.
 *
 * GetFullPathNameW makes no syscall (pure kernelbase path
 * math); there is nothing to hook there.
 */

/* wrapper symbols (wrapper_x64.S / wrapper_x64.asm) */
void retrace_wrap_fopen(void);
void retrace_wrap_open(void);
void retrace_wrap_close(void);
void retrace_wrap_read(void);
void retrace_wrap_write(void);
void retrace_wrap_lseek(void);
void retrace_wrap_stat(void);
void retrace_wrap_unlink(void);
void retrace_wrap_remove(void);
void retrace_wrap_rename(void);
void retrace_wrap_rmdir(void);
void retrace_wrap_NtCreateFile(void);
void retrace_wrap_NtOpenFile(void);
void retrace_wrap_NtQueryAttributesFile(void);
void retrace_wrap_NtClose(void);
void retrace_wrap_LdrLoadDll(void);

/*
 * Resolve every enabled target (GetProcAddress on the listed
 * module) and install its hook. Refused hooks (unsafe prologue,
 * missing export) are logged and skipped. Call AFTER
 * retrace_core_boot() so intercepted calls find initialized
 * registries. Returns the number of hooks installed.
 */
int retrace_win_install_hooks(void);

/* Uninstall all hooks installed by retrace_win_install_hooks. */
void retrace_win_uninstall_hooks(void);

/*
 * The last hook-refusal reason ("none" when everything installed
 * or nothing was attempted). For tests/diagnostics.
 */
const char *retrace_win_last_refusal(void);

/*
 * The trampoline for a hooked function -- its REAL
 * implementation (relocated prologue + jump back into the
 * body). NULL when the function is not hooked.
 */
void *retrace_win_trampoline_for(const char *func_name);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_BACKENDS_WIN_COMMON_HOOK_TARGETS_H_ */
