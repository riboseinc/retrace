/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */
#ifndef RETRACE_CORE_OTLP_ALLOCATOR_H_
#define RETRACE_CORE_OTLP_ALLOCATOR_H_

#include <stddef.h>

/*
 * The otlp-c allocator shim (TODO.trace-profile/31-32 Wave B/C;
 * extracted in the architecture deepening).
 *
 * otlp-c routes every internal allocation through the hooks
 * installed via otlp_set_allocator(). retrace's real_impls
 * exposes malloc/free but NOT realloc -- and a naive
 * malloc+memcpy+free shim cannot know the OLD block size, so it
 * would read past the end of the old allocation (heap overflow:
 * segfaults on musl, corrupts on glibc; the v2.35.0 lesson).
 *
 * The shim therefore prefixes every block with an 8-byte size
 * header: alloc writes it, realloc reads it to bound the copy,
 * free skips it. Payload stays 8-byte aligned (malloc's 16 + 8),
 * which satisfies otlp-c's void*-alignment contract.
 *
 * install() wires the trio into otlp-c; it must be called before
 * any other otlp-c API (allocations that reach libc directly
 * would re-enter the engine).
 *
 * No engine state: unit-testable in isolation (and reused by the
 * planned supervisor agent, which links otlp-c the same way).
 */
int retrace_otlp_allocator_install(void);

/*
 * The installed trio, for this module's own tests (the shim's
 * realloc bounding is the subtle part; the tests call the
 * hooks directly). Valid after install().
 */
struct retrace_otlp_allocator_hooks {
	void *(*alloc)(size_t n);
	void (*free)(void *p);
	void *(*realloc)(void *p, size_t n);
};

void retrace_otlp_allocator_hooks(
	struct retrace_otlp_allocator_hooks *out);

#endif /* RETRACE_CORE_OTLP_ALLOCATOR_H_ */
