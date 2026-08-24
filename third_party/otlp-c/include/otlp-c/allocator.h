/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Custom allocator hook. Allows the caller to replace the library's
 * internal malloc/free/realloc with a custom implementation.
 *
 * Use case: kernel modules, firmware, language VMs, and other
 * embedding contexts where the system allocator is unavailable or
 * inappropriate.
 *
 * Thread-safety: otlp_set_allocator must be called BEFORE any other
 * otlp-c function and must NOT be changed while the library is in
 * use. The typical pattern is to set it once at process startup.
 */
#ifndef OTLP_C_ALLOCATOR_H
#define OTLP_C_ALLOCATOR_H

#include <stddef.h>

#include "visibility.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*otlp_alloc_fn)(size_t size);
typedef void *(*otlp_realloc_fn)(void *ptr, size_t size);
typedef void  (*otlp_free_fn)(void *ptr);

typedef struct {
	otlp_alloc_fn   alloc;
	otlp_realloc_fn realloc;
	otlp_free_fn    free;
} otlp_allocator_t;

/* Set the global allocator used by all otlp-c internals. Pass NULL
 * to reset to the system defaults (malloc, realloc, free).
 *
 * Must be called before any other otlp-c function. Changing the
 * allocator while the library is in use is undefined behavior. */
OTLP_C_EXPORT
void otlp_set_allocator(const otlp_allocator_t *alloc);

/* Get the current allocator. Returns the system defaults if
 * otlp_set_allocator has not been called. */
OTLP_C_EXPORT
const otlp_allocator_t *otlp_get_allocator(void);

#ifdef __cplusplus
}
#endif

#endif
