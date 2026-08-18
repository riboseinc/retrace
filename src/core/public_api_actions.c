/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * The __retrace_acts half of the registry listing. Kept in its
 * own TU: the section-walk macro's extern declarations cannot
 * share a translation unit with another section's (see
 * public_api_internal.h).
 */

#include "public_api_internal.h"
#include "actions.h"

static const char *const *g_act_names;
static size_t g_act_count;

RETRACE_API retrace_status_t retrace_list_actions(
		const char *const **out_names, size_t *out_count)
{
	const struct Action *p;
	unsigned long size;

	if (out_names == NULL || out_count == NULL)
		return RETRACE_ERR_INVAL;

	retrace_as_get_section_info("__DATA", "__retrace_acts", &p, &size);
	return retrace_names_from_section(p, size, sizeof(*p),
		&g_act_names, &g_act_count, out_names, out_count);
}
