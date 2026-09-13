/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * The link-map real-impl fallback (TODO.impl/13).
 *
 * RTLD_NEXT searches only the CALLING object's dependency
 * chain. A provider the HOST loads itself (libssl, dlopened
 * by an extension module) never rides libretrace's chain, so
 * the probe returns NULL forever -- and the engine's NULL bail
 * synthesized -1 as the return, handing every caller a poison
 * pointer. The walk: enumerate every loaded object, skip
 * ourselves and the main executable, ask each by NOLOAD
 * handle. The first hit is the provider's own definition --
 * our preload export is never seen because we are skipped.
 *
 * Cached by the engine's name-slot cache: the walk runs once
 * per name, not per call.
 */

#define _GNU_SOURCE
#include <stddef.h>

#include <dlfcn.h>
#include <link.h>

#include "real_linkmap.h"

void *retrace_as_real_from_linkmap(const char *name)
{
	struct link_map *lm;
	void *self_base;
	Dl_info di;

	if (dladdr((void *)&retrace_as_real_from_linkmap, &di) == 0)
		return NULL;
	self_base = di.dli_fbase;

	if (dlinfo(dlopen(NULL, RTLD_LAZY), RTLD_DI_LINKMAP, &lm) != 0)
		return NULL;

	for (; lm != NULL; lm = lm->l_next) {
		void *h;
		void *p;

		/* the main executable: non-PIE carries l_addr == 0;
		 * PIE carries a nonzero bias AND an empty l_name.
		 * Either shape must be skipped -- a dlsym on the
		 * main handle follows its dependency chain, and the
		 * preload sits at its head: the "real" impl would be
		 * our own wrapper (the infinite recursion)
		 */
		if (lm->l_addr == 0 || lm->l_name[0] == '\0')
			continue;
		if ((void *)lm->l_addr == self_base)
			continue;	/* ourselves: our export IS the wrapper */
		h = dlopen(lm->l_name, RTLD_LAZY | RTLD_NOLOAD);
		if (h == NULL)
			continue;
		p = dlsym(h, name);
		if (p != NULL)
			return p;
	}
	return NULL;
}
