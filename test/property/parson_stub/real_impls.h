/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Stub real_impls.h for the property-based tests. Parson (the
 * vendored JSON library) uses retrace_real_impls.real_sprintf for
 * one call in json_serialize. In the test harness there is no
 * engine, so map the function pointer to libc directly.
 *
 * This matches the pattern in src/cli/real_impls.h.
 */
#ifndef RETRACE_TEST_PROPERTY_REAL_IMPLS_H
#define RETRACE_TEST_PROPERTY_REAL_IMPLS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct retrace_real_impls_tag {
	int (*real_sprintf)(char *buf, const char *fmt, ...);
	void *(*malloc)(size_t sz);
	void (*free)(void *ptr);
};

static const struct retrace_real_impls_tag retrace_real_impls = {
	sprintf,
	malloc,
	free,
};

#endif
