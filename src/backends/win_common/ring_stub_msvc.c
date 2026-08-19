/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * TODO.windows/04 staging: MSVC has no C11 stdatomic, so the
 * lock-free ring and its flusher do not compile there this
 * slice. These stubs satisfy the logger's API: ring init
 * fails, the logger falls back to the synchronous write path
 * (correct, hotter on the writing thread). Item 05 wires
 * Interlocked-based atomics and the real ring.
 */

#if defined(_MSC_VER) && !defined(__clang__)

#include "log_ring.h"
#include "log_flusher.h"

#include <stddef.h>
#include <stdint.h>

int retrace_log_ring_init(void)
{
	return -1;
}

void retrace_log_ring_deinit(void)
{
}

struct LogRing *retrace_log_ring_get(void)
{
	return NULL;
}

int retrace_log_ring_push(struct LogRing *ring, uint8_t module,
	uint8_t severity, uint32_t timestamp, const char *text)
{
	(void)ring;
	(void)module;
	(void)severity;
	(void)timestamp;
	(void)text;
	return -1;
}

size_t retrace_log_ring_drain(struct LogRing *ring,
	retrace_log_ring_drain_cb cb, void *ctx)
{
	(void)ring;
	(void)cb;
	(void)ctx;
	return 0;
}

void retrace_log_ring_walk(retrace_log_ring_walk_cb cb, void *ctx)
{
	(void)cb;
	(void)ctx;
}

uint64_t retrace_log_ring_total_dropped(void)
{
	return 0;
}

int retrace_log_flusher_init(
	retrace_log_flusher_emit_cb emit, void *ctx)
{
	(void)emit;
	(void)ctx;
	return -1;
}

void retrace_log_flusher_stop(void)
{
}

#endif /* MSVC only */

/*
 * call_hash.c uses C11 stdatomic, which MSVC gates behind
 * /experimental:c11atomics; stubbed alongside the ring
 * (TODO.windows/05 wires the real thing).
 */
#include "call_hash.h"

int retrace_call_hash_init(void)
{
	return -1;
}

int retrace_call_hash_enabled(void)
{
	return 0;
}

void retrace_call_hash_observe(const char *func_name, int arg_count)
{
	(void)func_name;
	(void)arg_count;
}

uint64_t retrace_call_hash_get(void)
{
	return 0;
}

void retrace_call_hash_deinit(void)
{
}

void retrace_call_hash_walk(retrace_call_hash_walk_cb cb, void *ctx)
{
	(void)cb;
	(void)ctx;
}
