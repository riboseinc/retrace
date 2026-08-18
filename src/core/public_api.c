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

#include "funcs.h"
#include "actions.h"
#include "real_impls.h"

#include <stddef.h>

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

/*
 * Registry listing: the section walk for __retrace_funcs must
 * live in its own TU (see public_api_internal.h). The actions
 * listing lives in public_api_actions.c for the same reason.
 */
#include "public_api_internal.h"
#include "funcs.h"

static const char *const *g_fn_names;
static size_t g_fn_count;

RETRACE_API retrace_status_t retrace_list_functions(
		const char *const **out_names, size_t *out_count)
{
	struct FuncPrototype *p;
	unsigned long size;

	if (out_names == NULL || out_count == NULL)
		return RETRACE_ERR_INVAL;

	retrace_as_get_section_info("__DATA", "__retrace_funcs", &p, &size);
	return retrace_names_from_section(p, size, sizeof(*p),
		&g_fn_names, &g_fn_count, out_names, out_count);
}
