/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Bump-allocator arena. Single-grow buffer with no per-allocation
 * headers. Designed for the OTLP encoder's per-POST sub-message
 * lifecycle: allocate a buffer, encode the batch, send it, free
 * once. No free-list, no per-object bookkeeping. Free is O(1).
 *
 * Failure mode: NOMEM propagates from append_alloc; the caller
 * disconnects, frees the arena, and drops the batch.
 */
#ifndef OTLP_C_ARENA_H
#define OTLP_C_ARENA_H

#include <otlp-c/status.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct otlp_arena {
	uint8_t *buf;
	size_t	 len;
	size_t	 cap;
	uint8_t	 initial_inline[256];  /* small-message optimisation */
	bool	 owns_buf;		    /* false while still in initial_inline */
};

otlp_status_t otlp_arena_init(struct otlp_arena *a);
void	    otlp_arena_free(struct otlp_arena *a);

/* Append `len` bytes from `src` to the arena. Returns OTLP_OK or
 * OTLP_ERR_NOMEM (grown as needed up to SIZE_MAX/2). */
otlp_status_t otlp_arena_append(struct otlp_arena *a,
				const uint8_t *src, size_t len);

/* Like otlp_arena_append but copies nothing — returns a pointer to
 * `len` zero-initialized bytes in the buffer. */
otlp_status_t otlp_arena_zero_extend(struct otlp_arena *a, size_t len,
				    uint8_t **out);

#endif
