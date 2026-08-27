/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_ENFORCE_SANDBOX_H_
#define RETRACE_ENFORCE_SANDBOX_H_

#include <stdio.h>

/*
 * Apply a Seatbelt (sandbox-exec) profile to THIS process's
 * children on macOS (01 P1). profile must be a NUL-terminated
 * (version 1) S-expression. The C API (sandbox_compile +
 * sandbox_apply) is private; the supported interface is the
 * /usr/bin/sandbox-exec wrapper, so this returns an argv the
 * caller execs INSTEAD of the target:
 *
 *   enforce_sandbox_exec_wrap(profile, argc, argv, out, cap)
 *     -> "/usr/bin/sandbox-exec -p '<profile>' -- <cmd...>"
 * assembled with proper quoting. Returns 0 built, -1 overflow.
 */
int enforce_sandbox_exec_wrap(const char *profile, int argc,
	char **argv, char *out, size_t cap);

#endif /* RETRACE_ENFORCE_SANDBOX_H_ */
