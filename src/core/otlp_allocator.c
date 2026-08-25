/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "otlp_allocator.h"

#include <string.h>

#include "real_impls.h"
#include "otlp-c/allocator.h"

/*
 * Push/pop the memcpy/free macros -- macOS's <string.h> maps
 * memcpy to __builtin___memcpy_chk under -O and the struct
 * field access in real_impls.memcpy would rewrite to the
 * builtin.
 */
#ifdef memcpy
#  pragma push_macro("memcpy")
#  pragma push_macro("free")
#  undef memcpy
#  undef free
#endif
#define OTLP_ALLOC_HDR_SZ sizeof(size_t)

static void *otlp_alloc(size_t n)
{
	unsigned char *blk = retrace_real_impls.malloc(
		n + OTLP_ALLOC_HDR_SZ);

	if (blk == NULL)
		return NULL;
	*(size_t *)blk = n;
	return blk + OTLP_ALLOC_HDR_SZ;
}

static void otlp_free(void *p)
{
	if (p != NULL)
		retrace_real_impls.free(
			(unsigned char *)p - OTLP_ALLOC_HDR_SZ);
}

static void *otlp_realloc(void *p, size_t n)
{
	unsigned char *blk;
	unsigned char *out;
	size_t old;

	if (p == NULL)
		return otlp_alloc(n);

	blk = (unsigned char *)p - OTLP_ALLOC_HDR_SZ;
	old = *(size_t *)blk;

	out = retrace_real_impls.malloc(n + OTLP_ALLOC_HDR_SZ);
	if (out == NULL)
		return NULL;
	*(size_t *)out = n;
	retrace_real_impls.memcpy(out + OTLP_ALLOC_HDR_SZ, p,
		old < n ? old : n);
	retrace_real_impls.free(blk);
	return out + OTLP_ALLOC_HDR_SZ;
}
#ifdef memcpy
#  pragma pop_macro("memcpy")
#  pragma pop_macro("free")
#endif

int retrace_otlp_allocator_install(void)
{
	otlp_allocator_t alloc = {
		.alloc = otlp_alloc,
		.realloc = otlp_realloc,
		.free = otlp_free,
	};

	otlp_set_allocator(&alloc);
	return 0;
}

void retrace_otlp_allocator_hooks(
	struct retrace_otlp_allocator_hooks *out)
{
	out->alloc = otlp_alloc;
	out->free = otlp_free;
	out->realloc = otlp_realloc;
}
