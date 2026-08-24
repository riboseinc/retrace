/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Slab allocator — fixed-slot-size memory pool with malloc fallback.
 *
 * Designed for hot loops that allocate many small, similarly-sized
 * objects (span clones in the exporter, attribute keys, event
 * names). Each slab pre-allocates a fixed arena of identically-sized
 * slots; allocations of `slot_size` or smaller are served from the
 * arena without calling malloc. Larger allocations and overflow
 * (all slots in use) transparently fall through to malloc, so the
 * API is a drop-in for any malloc/free pair.
 *
 * Thread-safety: NOT thread-safe. The slab is intended for
 * single-threaded hot paths. For cross-thread pooling, use one slab
 * per thread (or wrap with the caller's synchronization).
 *
 * Stats are tracked for observability — useful for tuning slot_size
 * and capacity.
 */
#ifndef OTLP_C_SLAB_H
#define OTLP_C_SLAB_H

#include <otlp-c/status.h>
#include <otlp-c/visibility.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct otlp_slab otlp_slab_t;

/* Observability counters. Zero-initialised; updated on every alloc/free. */
typedef struct {
	size_t slot_size;
	size_t capacity;
	size_t in_use;		/* slots currently handed out */
	size_t alloc_count;	/* total alloc calls */
	size_t slab_hits;	/* served from slab (no malloc) */
	size_t malloc_fallbacks; /* fell through to malloc (oversize or full) */
	size_t free_count;
	size_t slab_free_hits;	 /* returned to slab */
	size_t malloc_free_fallbacks; /* malloc ptrs freed via free() */
} otlp_slab_stats_t;

/* Construct a slab. `slot_size` is the max byte size that will be
 * served from the arena. `capacity` is the number of slots.
 *
 * Both allocate memory: `slot_size * capacity` for the arena plus
 * `capacity` bytes for the used-bitmap. Returns NULL on allocation
 * failure. */
OTLP_C_EXPORT
otlp_slab_t *otlp_slab_create(size_t slot_size, size_t capacity);

/* Destroy a slab. Does NOT free pointers still handed out — caller
 * must free them via otlp_slab_free_ptr first. Safe to call with NULL. */
OTLP_C_EXPORT
void otlp_slab_free(otlp_slab_t *slab);

/* Allocate `size` bytes. If `size <= slot_size` and a slot is free,
 * returns a slot from the arena. Otherwise calls malloc(size) and
 * returns that. Returns NULL on malloc failure.
 *
 * The returned pointer is guaranteed aligned to at least the
 * alignment of `void *`. */
OTLP_C_EXPORT
void *otlp_slab_alloc(otlp_slab_t *slab, size_t size);

/* Free a pointer previously returned by otlp_slab_alloc. Detects
 * whether the pointer is from the slab's arena (returns to free
 * list) or from malloc (calls free). Safe with NULL. */
OTLP_C_EXPORT
void otlp_slab_free_ptr(otlp_slab_t *slab, void *ptr);

/* Snapshot the slab's stats. Safe with NULL slab (writes zeros). */
OTLP_C_EXPORT
void otlp_slab_get_stats(const otlp_slab_t *slab, otlp_slab_stats_t *out);

/* Install a slab-backed allocator as the process-wide allocator for
 * the otlp-c library. Subsequent otlp_malloc / otlp_free calls
 * route through the slab for allocations <= slot_size; everything
 * else falls through to the previously-installed allocator.
 *
 * Returns OTLP_ERR_NOMEM if the arena can't be allocated.
 *
 * The slab is single-threaded by design (see file comment). This
 * install function is intended for embedding scenarios where the
 * caller controls threading — typically install once at startup,
 * uninstall at shutdown. */
OTLP_C_EXPORT
otlp_status_t otlp_install_slab_allocator(size_t slot_size, size_t capacity);

/* Uninstall the slab allocator; restores the previous allocator.
 * Frees the arena. Pointers returned by otlp_malloc before this
 * call must be freed BEFORE calling uninstall — their slots are
 * invalidated when the arena is freed. */
OTLP_C_EXPORT
void otlp_uninstall_slab_allocator(void);

#ifdef __cplusplus
}
#endif

#endif
