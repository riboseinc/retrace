/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_BACKENDS_PRELOAD_ELF_REAL_LINKMAP_H_
#define RETRACE_BACKENDS_PRELOAD_ELF_REAL_LINKMAP_H_

/*
 * Resolve a symbol from any loaded PROVIDER (link-map walk,
 * self-skipping). See real_linkmap.c for why RTLD_NEXT is not
 * enough for host-loaded providers.
 */
void *retrace_as_real_from_linkmap(const char *name);

#endif /* RETRACE_BACKENDS_PRELOAD_ELF_REAL_LINKMAP_H_ */
