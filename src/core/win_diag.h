/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_CORE_WIN_DIAG_H_
#define RETRACE_CORE_WIN_DIAG_H_

/*
 * RETRACE_WIN_DIAG breadcrumbs (TODO.trace-profile/07): the last
 * tag printed before a crash names the failing step. Win32-only
 * I/O (WriteFile) -- NO CRT syscalls on the traced path, so the
 * breadcrumbs cannot recurse through the hooks. Zero cost when
 * the env var is unset; POSIX builds compile to nothing.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

static inline void retrace_win_diag(const char *tag, const char *name,
				    long long n)
{
	static int enabled = -1;
	char buf[128];
	int len;
	DWORD wrote = 0;
	HANDLE h;

	if (enabled < 0) {
		/* Win32 ONLY: getenv is a HOOKED export (v2.16.0) --
		 * a CRT call here recurses wrapper->diag->getenv->
		 * wrapper before the latch commits (stack overflow)
		 */
		char buf[8];

		enabled = GetEnvironmentVariableA(
			"RETRACE_WIN_DIAG", buf, sizeof(buf)) > 0 &&
			buf[0] == '1';
	}
	if (!enabled)
		return;
	len = snprintf(buf, sizeof(buf), "wd: %s %s %lld\n", tag,
		name != NULL ? name : "", n);
	h = GetStdHandle(STD_OUTPUT_HANDLE);
	if (len > 0)
		WriteFile(h, buf, (DWORD)len, &wrote, NULL);
}

#else

static inline void retrace_win_diag(const char *tag, const char *name,
				    long long n)
{
	(void)tag;
	(void)name;
	(void)n;
}

#endif /* _WIN32 */

#endif /* RETRACE_CORE_WIN_DIAG_H_ */
