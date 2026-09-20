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
#include "real_impls.h"

#ifdef __ANDROID__
/*
 * Bionic has no dlinfo/RTLD_DI_LINKMAP: enumerate loaded
 * objects with dl_iterate_phdr (bionic provides it) and
 * probe each by name -- the same NOLOAD+dlsym discipline as
 * the link_map walk below.
 */
struct android_walk_ctx {
	const char *name;
	void *self_base;
	void *found;
};

static int android_probe_cb(struct dl_phdr_info *info,
	size_t size, void *data)
{
	struct android_walk_ctx *ctx = data;
	void *h;
	void *p;

	(void) size;
	if (info->dlpi_name == NULL || info->dlpi_name[0] == '\0')
		return 0;	/* main executable */
	if ((void *) info->dlpi_addr == ctx->self_base)
		return 0;	/* ourselves */
	if (retrace_real_impls.strncmp != NULL &&
		retrace_real_impls.strncmp(info->dlpi_name,
			"linux-vdso", 10) == 0)
		return 0;
	h = dlopen(info->dlpi_name, RTLD_LAZY | RTLD_NOLOAD);
	if (h == NULL)
		return 0;
	p = dlsym(h, ctx->name);
	if (p != NULL && (unsigned long) p >= 0x100000000UL) {
		ctx->found = p;
		return 1;
	}
	return 0;
}

void *retrace_as_real_from_linkmap(const char *name)
{
	Dl_info di;
	struct android_walk_ctx ctx;

	if (dladdr((void *)&retrace_as_real_from_linkmap, &di) == 0)
		return NULL;
	ctx.name = name;
	ctx.self_base = di.dli_fbase;
	ctx.found = NULL;
	dl_iterate_phdr(android_probe_cb, &ctx);
	return ctx.found;
}
#else /* glibc / musl link_map chain */

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
		/* the vdso: its (qemu-provided) symtab is not a
		 * full symbol table -- under qemu-user gdb itself
		 * reports "corrupt string table index" for it, and
		 * dlsym against it returned garbage offsets that the
		 * call_real tail then jumped to (the 0x22325c ghost)
		 */
		if (retrace_real_impls.strncmp != NULL &&
			retrace_real_impls.strncmp(lm->l_name,
				"linux-vdso", 10) == 0)
			continue;
		h = dlopen(lm->l_name, RTLD_LAZY | RTLD_NOLOAD);
		if (h == NULL)
			continue;
		p = dlsym(h, name);
		/* same sanity floor as get_real_safe: the loader's
		 * lookup paths have been observed returning low
		 * garbage under qemu-ppc64le preloads
		 */
		if (p != NULL && (unsigned long) p >= 0x100000000UL)
			return p;
	}
	return NULL;
}
#endif /* __ANDROID__ */
