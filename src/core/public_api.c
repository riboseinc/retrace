/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * The implemented surface of <retrace/retrace.h>. Each function
 * declared in the public header has its definition here or in the
 * owning subsystem (registry.c owns the backend/attach entry
 * points); test_public_api.c dlsyms every declaration so a
 * declared-but-unlinked symbol fails the build.
 */

#include <retrace/retrace.h>
#include <retrace/version.h>

RETRACE_API const char *retrace_version(void)
{
	return RETRACE_VERSION_STRING;
}

RETRACE_API const char *retrace_version_info(void)
{
	return "retrace " RETRACE_VERSION_STRING
	       " -- userspace libc interceptor"
	       " (https://github.com/riboseinc/retrace)";
}
