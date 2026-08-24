/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Lock-free bounded MPSC ring. See src/mpsc_queue.h.
 *
 * Uses Dmitry Vyukov's canonical bounded-queue sequence scheme:
 *
 *   init: slot[i].seq = i
 *
 *   push(h):  diff = slot.seq - h
 *             diff == 0 → slot empty for this turn; CAS head, write data,
 *                         release slot.seq = h + 1
 *             diff < 0  → queue full (slot not yet consumed by the time
 *                         the producer wrapped to it)
 *
 *   pop(t):   diff = slot.seq - (t + 1)
 *             diff == 0 → slot published; read data, advance tail,
 *                         release slot.seq = t + capacity (so the next
 *                         producer turn at h = t + capacity sees
 *                         slot.seq == h, i.e. diff == 0 again)
 *             diff < 0  → queue empty
 *
 * The scheme is correct because: after a push at turn h, the slot's
 * sequence is h + 1. When the producer wraps (h reaches t + capacity
 * without an intervening pop), slot.seq is still h_old + 1 which is
 * far below the new h, so diff < 0 (full). Only after the consumer
 * pops at t (setting slot.seq = t + capacity = h_new) does the slot
 * become available for the wrapped producer.
 *
 * Atomic operations are wrapped through src/atomic_compat.h so the
 * library compiles on MSVC, whose <stdatomic.h> is unreliable
 * across VS preview versions.
 */
#include "mpsc_queue.h"
#include "atomic_compat.h"
#include "internal_util.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static bool
is_pow2(size_t n)
{
	return n > 0 && (n & (n - 1)) == 0;
}

otlp_status_t
mpsc_queue_init(struct mpsc_queue *q, size_t capacity)
{
	size_t i;

	if (!q || !is_pow2(capacity))
		return OTLP_ERR_INVALID_ARGUMENT;
	/* Overflow check: capacity * sizeof(slot) must not wrap. Without
	 * this, a huge capacity produces an undersized slots array. */
	if (capacity > SIZE_MAX / sizeof(*q->slots))
		return OTLP_ERR_INVALID_ARGUMENT;

	q->slots = otlp_calloc(capacity, sizeof(*q->slots));
	if (!q->slots)
		return OTLP_ERR_NOMEM;
	q->mask = capacity - 1;
	otlp_atomic_store_u64(&q->head, 0, OTLP_MEMORY_ORDER_RELAXED);
	otlp_atomic_store_u64(&q->tail, 0, OTLP_MEMORY_ORDER_RELAXED);
	for (i = 0; i < capacity; i++)
		otlp_atomic_store_u64(
			&q->slots[i].seq, i, OTLP_MEMORY_ORDER_RELAXED);
	return OTLP_OK;
}

void
mpsc_queue_free(struct mpsc_queue *q)
{
	if (!q)
		return;
	otlp_free(q->slots);
	q->slots = NULL;
	q->mask = 0;
}

otlp_status_t
mpsc_queue_push(struct mpsc_queue *q, void *item)
{
	uint64_t h;
	struct mpsc_slot *slot;
	uint64_t seq;

	h = otlp_atomic_load_u64(&q->head, OTLP_MEMORY_ORDER_RELAXED);
	for (;;)
	{
		slot = &q->slots[h & q->mask];
		seq = otlp_atomic_load_u64(&slot->seq, OTLP_MEMORY_ORDER_ACQUIRE);
		const int64_t diff = (int64_t) seq - (int64_t) h;
		if (diff == 0)
		{
			if (otlp_atomic_cas_u64(&q->head,
					    &h,
					    h + 1,
					    OTLP_MEMORY_ORDER_RELAXED,
					    OTLP_MEMORY_ORDER_RELAXED))
				break;
		}
		else if (diff < 0)
		{
			return OTLP_ERR_BUFFER_FULL;
		}
		else
		{
			h = otlp_atomic_load_u64(
				&q->head, OTLP_MEMORY_ORDER_RELAXED);
		}
	}

	slot->data = item;
	otlp_atomic_store_u64(
		&slot->seq, h + 1, OTLP_MEMORY_ORDER_RELEASE);
	return OTLP_OK;
}

void *
mpsc_queue_pop(struct mpsc_queue *q)
{
	uint64_t t = otlp_atomic_load_u64(&q->tail, OTLP_MEMORY_ORDER_RELAXED);
	struct mpsc_slot *slot = &q->slots[t & q->mask];
	uint64_t seq = otlp_atomic_load_u64(&slot->seq, OTLP_MEMORY_ORDER_ACQUIRE);
	const int64_t diff = (int64_t) seq - (int64_t) (t + 1);

	if (diff != 0)
		return NULL;

	void *data = slot->data;
	otlp_atomic_store_u64(
		&slot->seq, t + q->mask + 1, OTLP_MEMORY_ORDER_RELEASE);
	otlp_atomic_store_u64(&q->tail, t + 1, OTLP_MEMORY_ORDER_RELAXED);
	return data;
}

size_t
mpsc_queue_size(const struct mpsc_queue *q)
{
	/* Cast away const: atomic_load is conceptually a read but the
	 * wrapper's pointer is non-const (the underlying atomic type
	 * is mutable). Safe — the operation does not modify q. */
	struct mpsc_queue *mq = (struct mpsc_queue *) q;
	uint64_t h = otlp_atomic_load_u64(&mq->head, OTLP_MEMORY_ORDER_RELAXED);
	uint64_t t = otlp_atomic_load_u64(&mq->tail, OTLP_MEMORY_ORDER_RELAXED);

	return h >= t ? (size_t) (h - t) : 0;
}
