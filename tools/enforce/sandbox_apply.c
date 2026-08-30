/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Seatbelt application (TODO.beyond-libc/01 P1, macOS). The
 * C sandbox API (sandbox_compile_string/sandbox_apply) is
 * private and version-fragile; the SUPPORTED interface is
 * /usr/bin/sandbox-exec, so the installer wraps the exec:
 * sandbox-exec -p '<profile>' -- <cmd>. The profile carries
 * no single quotes (our generator emits only paths and
 * parentheses) -- quoting stays trivial by construction.
 */

#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "sandbox_apply.h"

#ifdef __APPLE__

int enforce_sandbox_exec_wrap(const char *profile, int argc,
	char **argv, char *out, size_t cap)
{
	size_t o = 0;
	int i;
	int n;

	n = snprintf(out + o, cap - o,
		"/usr/bin/sandbox-exec -p '%s' --", profile);
	if (n <= 0 || (size_t)n >= cap - o)
		return -1;
	o += (size_t)n;
	for (i = 0; i < argc; i++) {
		n = snprintf(out + o, cap - o, " '%s'", argv[i]);
		if (n <= 0 || (size_t)n >= cap - o)
			return -1;
		o += (size_t)n;
	}
	return 0;
}

#else

int enforce_sandbox_exec_wrap(const char *profile, int argc,
	char **argv, char *out, size_t cap)
{
	(void)profile;
	(void)argc;
	(void)argv;
	(void)out;
	(void)cap;
	return -1;
}

#endif
