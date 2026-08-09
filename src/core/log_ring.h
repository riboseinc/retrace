/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RETRACE_CORE_LOG_RING_H_
#define RETRACE_CORE_LOG_RING_H_

#include <stddef.h>
#include <stdint.h>

/*
 * Lock-free per-thread log ring (TODO.complete/19 P0).
 *
 * Single-producer (the owning thread), single-consumer (the background
 * flusher). Capacity is fixed (power-of-2) so wrap uses a bitmask
 * instead of a modulo. Hot-path push is two relaxed loads, one store
 * with release semantics -- no CAS, no locks.
 *
 * The ring is intended for the logger hot path: every intercepted
 * libc call may emit one or more entries, so the per-event cost
 * dominates retrace's overhead. Replacing the current global-mutex
 * logger with these rings + a background flusher takes the ceiling
 * from ~10K events/sec (mutex-bound) to ~100K+ events/sec
 * (per-thread parallel writes + batched drain).
 *
 * Memory layout: each thread lazily allocates its ring on first
 * push via a pthread_key destructor. The flusher walks the list of
 * registered rings at a 1ms cadence and drains each.
 *
 * Backpressure: when the ring is full, push() drops the new entry
 * and increments the ring's dropped counter. The flusher surfaces
 * the dropped count as a final summary entry at deinit. Future
 * modes (block / sample) will be configurable via RETRACE_LOGGER_BP.
 */

#include "logger.h"

/*
 * One ring entry. Fixed-size so the ring is a flat array (cache
 * friendly, no per-entry allocation).
 *
 *   4 bytes : timestamp (ms since logger init, uint32 wraps at ~49d)
 *   1 byte  : module (enum Modules from logger.h)
 *   1 byte  : severity (enum Severity from logger.h)
 *   2 bytes : reserved (alignment)
 *   248 bytes: formatted text (NUL-terminated within this field)
 *
 * Total: 256 bytes. Messages longer than 247 chars are truncated
 * to fit; the trailing '\0' is always written.
 */
#define LOG_RING_TEXT_CAP 248

struct LogEntry {
	uint32_t ts_ms;
	uint8_t  module;
	uint8_t  sev;
	uint16_t _reserved;
	char     text[LOG_RING_TEXT_CAP];
};

/* Default capacity (power-of-2). 64 entries × 256 B = 16 KB per thread. */
#define LOG_RING_DEFAULT_CAP 64

struct LogRing {
	uint32_t mask;             /* capacity - 1 */
	uint32_t dropped;          /* monotonic drop counter */
	_Atomic uint32_t head;     /* writer position */
	_Atomic uint32_t tail;     /* reader position */
	struct LogEntry *entries;  /* capacity entries */
};

/*
 * Module init / deinit. Init creates the per-thread pthread_key
 * and the global ring registry. Deinit walks the registry, frees
 * every ring (the flusher is expected to have drained them
 * already), and destroys the key.
 *
 * Returns 0 on success, -1 on failure (logs via log_err).
 */
int retrace_log_ring_init(void);
void retrace_log_ring_deinit(void);

/*
 * Get the calling thread's ring, allocating it on first call.
 * Returns NULL only on out-of-memory. The thread owns its ring
 * until exit; the pthread_key destructor hands it to the global
 * registry for the flusher to drain one last time before freeing.
 *
 * The returned pointer is NOT owned by the caller -- do not free.
 */
struct LogRing *retrace_log_ring_get(void);

/*
 * Push an entry onto the ring. Returns:
 *   0  on success
 *   -1 if the ring was full (entry dropped, `ring->dropped` bumped)
 *
 * The text is copied into the ring entry. Entries longer than
 * LOG_RING_TEXT_CAP - 1 are truncated; the truncation is silent
 * (the flusher can detect it by checking strlen(entry->text) but
 * in practice log messages fit comfortably).
 */
int retrace_log_ring_push(struct LogRing *ring, uint8_t module,
			  uint8_t sev, uint32_t ts_ms, const char *text);

/*
 * Drain callback. Called once per available entry. Returns 0 to
 * continue draining, non-zero to stop early.
 */
typedef int (*retrace_log_ring_drain_cb)(const struct LogEntry *entry,
					 void *ctx);

/*
 * Drain all available entries from the ring, calling cb for each.
 * Returns the number of entries drained (0 if the ring was empty
 * or cb stopped early).
 *
 * Safe to call concurrently with push (SPSC contract). Not safe
 * for two flushers to call drain on the same ring simultaneously
 * -- the flusher thread is unique by design.
 */
size_t retrace_log_ring_drain(struct LogRing *ring,
			      retrace_log_ring_drain_cb cb, void *ctx);

/*
 * Walk every registered ring and call `cb(ring, ctx)` for each.
 * Used by the flusher to iterate. The registry is walkable
 * without locks because rings are appended atomically; new rings
 * become visible to the flusher on the next walk.
 */
typedef void (*retrace_log_ring_walk_cb)(struct LogRing *ring, void *ctx);

void retrace_log_ring_walk(retrace_log_ring_walk_cb cb, void *ctx);

/*
 * Total dropped count across all registered rings. Called by
 * the flusher at deinit for the "events lost" summary line.
 */
uint64_t retrace_log_ring_total_dropped(void);

#endif /* RETRACE_CORE_LOG_RING_H_ */
