/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Lock-free bounded MPSC ring. Vyukov-style with per-slot sequence
 * numbers — the canonical pattern for multi-producer/single-consumer
 * queues on C11 atomics. No mutexes.
 *
 * Producers (any thread) call mpsc_queue_push(); if the queue is
 * full, returns OTLP_ERR_BUFFER_FULL (back-pressure).
 *
 * Consumer (single thread — the exporter's tick) calls
 * mpsc_queue_pop() repeatedly until it returns NULL.
 *
 * Capacity is fixed at construction. The exporter pre-allocates one
 * of these with capacity 4096 by default; see otlp_exporter_opts_t.
 */
#ifndef OTLP_C_MPSC_QUEUE_H
#define OTLP_C_MPSC_QUEUE_H

#include <otlp-c/status.h>

#include "atomic_compat.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mpsc_slot
{
	otlp_atomic_u64 seq;
	void *data;
};

struct mpsc_queue
{
	struct mpsc_slot *slots;
	size_t mask; /* capacity - 1; capacity must be pow2 */
	otlp_atomic_u64 head; /* producer-side index */
	otlp_atomic_u64 tail; /* consumer-side index */
};

/* Initialise q with `capacity` slots. Capacity must be a power of
 * two. Returns OTLP_ERR_INVALID_ARGUMENT if not, OTLP_ERR_NOMEM on
 * allocation failure. */
otlp_status_t
mpsc_queue_init(struct mpsc_queue *q, size_t capacity);

/* Free the slots array. Does NOT drain queued items — the caller
 * pops (and disposes of) everything first, or the items leak. */
void
mpsc_queue_free(struct mpsc_queue *q);

/* Producer side. Returns OTLP_ERR_BUFFER_FULL if the queue is at
 * capacity (caller should drop or backoff). */
otlp_status_t
mpsc_queue_push(struct mpsc_queue *q, void *item);

/* Consumer side (single thread only). Returns the next item, or
 * NULL when empty. */
void *
mpsc_queue_pop(struct mpsc_queue *q);

/* Approximate size, for diagnostics. May race with producers. */
size_t
mpsc_queue_size(const struct mpsc_queue *q);

#endif
