/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Slab allocator implementation. See include/otlp-c/slab.h.
 *
 * Design: a contiguous arena of `slot_size * capacity` bytes, with
 * a parallel bitmap tracking which slots are in use. Allocations
 * up to slot_size are served from a free slot (linear scan); larger
 * allocations and overflow fall through to malloc. Free detects
 * whether the pointer lies within the arena and routes accordingly.
 *
 * Single-threaded by design (no atomics, no locks). The typical
 * caller is a per-tracer or per-thread slab used during span
 * construction (which is single-threaded by API contract).
 *
 * Slot size and capacity are caller-controlled. Sizing note: any
 * slot size is SAFE (the installed wrapper is arena-aware for
 * realloc — growing buffers move out of the slab, v0.5.85); for a
 * useful hit rate pick slot_size around the common small
 * allocation (attribute keys/values, small vectors ~128-256
 * bytes; spans themselves are ~176 bytes).
 */
#include <otlp-c/slab.h>

#include "../include/otlp-c/allocator.h"
#include "internal_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define OTLP_SLAB_ALIGN _Alignof(void *)

struct otlp_slab
{
	size_t slot_size; /* user-requested size, rounded up to alignment */
	size_t capacity;
	uint8_t *arena; /* slot_size * capacity bytes */
	bool *used; /* capacity bytes */
	size_t in_use;
	/* Free-list stack: indices of available slots. Alloc pops,
	 * free pushes. O(1) per operation (was O(capacity) linear scan). */
	size_t *free_stack;
	size_t free_top; /* number of entries in free_stack */
	otlp_slab_stats_t stats;
};

/* ── Process-wide slab allocator integration ──────────────────── */

static otlp_slab_t *g_installed_slab = NULL;
static otlp_allocator_t g_prev_allocator;
static bool g_slab_installed = false;

static void *
slab_alloc_hook(size_t n)
{
	if (g_installed_slab && n <= g_installed_slab->slot_size)
		return otlp_slab_alloc(g_installed_slab, n);
	return g_prev_allocator.alloc(n);
}

static void
slab_free_hook(void *p)
{
	/* Inline the arena check here to avoid recursion: otlp_slab_free_ptr
	 * falls through to otlp_free for non-arena pointers, which would
	 * re-enter this hook. */
	if (g_installed_slab && p)
	{
		struct otlp_slab *s = g_installed_slab;
		uintptr_t base = (uintptr_t) s->arena;
		uintptr_t end = base + (s->slot_size * s->capacity);
		uintptr_t pp = (uintptr_t) p;

		if (pp >= base && pp < end)
		{
			size_t off = pp - base;
			size_t i = off / s->slot_size;

			if (i < s->capacity && s->used[i])
			{
				s->used[i] = false;
				s->in_use--;
				s->stats.free_count++;
				s->stats.slab_free_hits++;
				s->stats.in_use = s->in_use;
				s->free_stack[s->free_top++] = i;
				return;
			}
		}
	}
	g_prev_allocator.free(p);
}

/* Arena-aware realloc: a pointer served from the arena must be
 * MOVED (new allocation + copy + slot returned), never handed to
 * the underlying realloc — libc realloc on a slab pointer is
 * undefined behavior (v0.5.85; the pre-fix pass-through aborted
 * under macOS libmalloc). The copy is min(slot_size, n): the
 * caller's original request was <= slot_size, so the full live
 * content is preserved (a growth over-copies only dead bytes). */
static void *
slab_realloc_hook(void *p, size_t n)
{
	struct otlp_slab *s = g_installed_slab;
	uintptr_t base;
	uintptr_t end;
	uintptr_t pp;

	if (!s)
		return g_prev_allocator.realloc(p, n);
	if (!p)
		return slab_alloc_hook(n);
	if (n == 0)
	{
		slab_free_hook(p);
		return NULL;
	}
	base = (uintptr_t) s->arena;
	end = base + (s->slot_size * s->capacity);
	pp = (uintptr_t) p;
	if (pp >= base && pp < end)
	{
		void *np = slab_alloc_hook(n);
		size_t copy = s->slot_size < n ? s->slot_size : n;

		if (!np)
			return NULL;
		memcpy(np, p, copy);
		slab_free_hook(p);
		return np;
	}
	return g_prev_allocator.realloc(p, n);
}

otlp_status_t
otlp_install_slab_allocator(size_t slot_size, size_t capacity)
{
	otlp_slab_t *s;
	otlp_allocator_t wrapped;

	if (g_slab_installed)
		return OTLP_ERR_INVALID_ARGUMENT;
	s = otlp_slab_create(slot_size, capacity);
	if (!s)
		return OTLP_ERR_NOMEM;
	g_prev_allocator = *otlp_get_allocator();
	g_installed_slab = s;
	wrapped.alloc = slab_alloc_hook;
	wrapped.realloc = slab_realloc_hook;
	wrapped.free = slab_free_hook;
	otlp_set_allocator(&wrapped);
	g_slab_installed = true;
	return OTLP_OK;
}

void
otlp_uninstall_slab_allocator(void)
{
	if (!g_slab_installed)
		return;
	otlp_set_allocator(&g_prev_allocator);
	otlp_slab_free(g_installed_slab);
	g_installed_slab = NULL;
	g_slab_installed = false;
}

static size_t
round_up(size_t v, size_t align)
{
	return (v + align - 1) & ~(align - 1);
}

otlp_slab_t *
otlp_slab_create(size_t slot_size, size_t capacity)
{
	struct otlp_slab *s;
	size_t aligned_slot;

	if (slot_size == 0 || capacity == 0)
		return NULL;
	aligned_slot = round_up(slot_size, OTLP_SLAB_ALIGN);
	s = otlp_calloc(1, sizeof(*s));
	if (!s)
		return NULL;
	s->slot_size = aligned_slot;
	s->capacity = capacity;
	s->arena = otlp_malloc(aligned_slot * capacity);
	if (!s->arena)
		goto fail;
	s->used = otlp_calloc(capacity, sizeof(*s->used));
	if (!s->used)
		goto fail;
	s->free_stack = otlp_malloc(capacity * sizeof(*s->free_stack));
	if (!s->free_stack)
		goto fail;
	/* Initialise free-list: all slots available. */
	for (size_t i = 0; i < capacity; i++)
		s->free_stack[i] =
			capacity - 1 - i; /* reverse so slot 0 pops first */
	s->free_top = capacity;
	s->stats.slot_size = aligned_slot;
	s->stats.capacity = capacity;
	return s;
fail:
	otlp_free(s->free_stack);
	otlp_free(s->arena);
	otlp_free(s->used);
	otlp_free(s);
	return NULL;
}

void
otlp_slab_free(otlp_slab_t *slab)
{
	if (!slab)
		return;
	otlp_free(slab->arena);
	otlp_free(slab->used);
	otlp_free(slab->free_stack);
	otlp_free(slab);
}

static int
ptr_in_arena(const struct otlp_slab *s, void *ptr)
{
	uintptr_t base = (uintptr_t) s->arena;
	uintptr_t end = base + (s->slot_size * s->capacity);
	uintptr_t p = (uintptr_t) ptr;

	return p >= base && p < end;
}

void *
otlp_slab_alloc(otlp_slab_t *slab, size_t size)
{
	if (!slab)
		return NULL;
	slab->stats.alloc_count++;
	if (size <= slab->slot_size && slab->free_top > 0)
	{
		size_t i = slab->free_stack[--slab->free_top];
		slab->used[i] = true;
		slab->in_use++;
		slab->stats.in_use = slab->in_use;
		slab->stats.slab_hits++;
		return slab->arena + (i * slab->slot_size);
	}
	/* Overflow or oversize: fall through to libc malloc directly,
	 * NOT otlp_malloc. When the slab is installed as the global
	 * allocator, otlp_malloc would re-enter slab_alloc_hook →
	 * infinite recursion. libc malloc is always safe. */
	slab->stats.malloc_fallbacks++;
	return malloc(size);
}

void
otlp_slab_free_ptr(otlp_slab_t *slab, void *ptr)
{
	if (!slab || !ptr)
		return;
	slab->stats.free_count++;
	if (ptr_in_arena(slab, ptr))
	{
		size_t off =
			(size_t)((uintptr_t) ptr - (uintptr_t) slab->arena);
		size_t i = off / slab->slot_size;

		if (i < slab->capacity && slab->used[i])
		{
			slab->used[i] = false;
			slab->in_use--;
			slab->stats.in_use = slab->in_use;
			slab->stats.slab_free_hits++;
			slab->free_stack[slab->free_top++] = i;
			return;
		}
		/* Pointer is in arena range but not a valid in-use slot.
		 * Treat as a double-free or invalid pointer — silently
		 * ignore. Calling libc free() on an arena pointer is
		 * undefined behavior (arena is owned by the slab, not
		 * the libc allocator); the previous code's "defensive"
		 * free was UB. */
		return;
	}
	slab->stats.malloc_free_fallbacks++;
	/* Use libc free directly — matches the malloc() used in the
	 * alloc fallback (not otlp_free, which would re-enter the
	 * slab_free_hook when the slab is installed globally). */
	free(ptr);
}

void
otlp_slab_get_stats(const otlp_slab_t *slab, otlp_slab_stats_t *out)
{
	if (!out)
		return;
	if (!slab)
	{
		memset(out, 0, sizeof(*out));
		return;
	}
	*out = slab->stats;
}
