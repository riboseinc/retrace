/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_CORE_PUBLIC_API_INTERNAL_H_
#define RETRACE_CORE_PUBLIC_API_INTERNAL_H_

#include <retrace/retrace.h>

#include <stddef.h>
#include <stdlib.h>

/*
 * Shared builder for the registry-listing entry points. Both
 * struct FuncPrototype and struct Action begin with a char name[]
 * member, so the name array can be built generically from the
 * section base + element size.
 *
 * The per-section walk itself (the retrace_as_get_section_info
 * macro) must happen in the CALLING translation unit: the macro
 * declares block-scope externs that clang hoists to file scope,
 * so a single TU cannot walk two different sections. Hence one
 * small TU per public listing, mirroring funcs.c/actions.c.
 *
 * Returns RETRACE_OK and fills *out/*count (array malloc'd once,
 * cached in *cache; strings borrowed from static section data);
 * RETRACE_ERR_NOMEM on allocation failure.
 */
static inline retrace_status_t retrace_names_from_section(
		const void *base, size_t bytes, size_t elem_size,
		const char ***cache, size_t *cached_count,
		const char *const **out_names, size_t *out_count)
{
	size_t n = bytes / elem_size;

	if (*cache == NULL) {
		const char **names;
		size_t i;

		if (n == 0) {
			static const char *const empty[] = { NULL };

			*cache = empty;
			*cached_count = 0;
			*out_names = *cache;
			*out_count = 0;
			return RETRACE_OK;
		}
		names = (const char **)malloc(n * sizeof(char *));
		if (names == NULL)
			return RETRACE_ERR_NOMEM;
		for (i = 0; i < n; i++) {
			/* name is an embedded char[] at offset 0 of both
			 * structs -- the string IS base + i*elem_size,
			 * not a pointer stored there.
			 */
			names[i] = (const char *)base + i * elem_size;
		}
		*cache = names;
		*cached_count = n;
	}

	*out_names = *cache;
	*out_count = *cached_count;
	return RETRACE_OK;
}

#endif /* RETRACE_CORE_PUBLIC_API_INTERNAL_H_ */
