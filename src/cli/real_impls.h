/*
 * Stub real_impls for the CLI standalone build.
 *
 * parson.c references retrace_real_impls.real_sprintf, which in the
 * full library routes through the reentrancy-safe indirection (see
 * src/core/real_impls.h). The CLI is a standalone binary — NOT
 * running under retrace — so we map directly to standard libc.
 *
 * This file is found before the real real_impls.h because the CLI's
 * source directory is first on the include path.
 */
#ifndef CLI_REAL_IMPLS_STUB_H_
#define CLI_REAL_IMPLS_STUB_H_

#include <stdio.h>

struct RetraceRealImpls {
	int (*real_sprintf)(char *str, const char *format, ...);
};

static struct RetraceRealImpls retrace_real_impls = {
	.real_sprintf = sprintf,
};

#endif
