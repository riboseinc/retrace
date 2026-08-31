/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "agent_ring.h"

#include <stdlib.h>
#include <string.h>

void agent_ring_init(struct agent_ring *ring)
{
	memset(ring, 0, sizeof(*ring));
}

static int ring_push(struct agent_ring *ring, char *heap_item,
	const char *item, size_t len)
{
	struct agent_ring_slot *slot;

	if (ring->count >= AGENT_RING_CAP) {
		ring->dropped++;
		free(heap_item);
		return -1;
	}
	slot = &ring->slots[(ring->head + ring->count) %
		AGENT_RING_CAP];
	slot->heap = NULL;
	if (heap_item != NULL) {
		slot->heap = heap_item;
	} else if (len > AGENT_RING_INLINE - 1) {
		slot->heap = (char *)malloc(len + 1);
		if (slot->heap == NULL) {
			/* allocation failure is a refusal too --
			 * counted, never silent
			 */
			ring->dropped++;
			return -1;
		}
		memcpy(slot->heap, item, len);
		slot->heap[len] = '\0';
	} else {
		memcpy(slot->buf, item, len);
		slot->buf[len] = '\0';
	}
	slot->len = len;
	ring->count++;
	return 0;
}

int agent_ring_push_copy(struct agent_ring *ring, const char *item,
	size_t len)
{
	return ring_push(ring, NULL, item, len);
}

int agent_ring_push_heap(struct agent_ring *ring, char *heap_item,
	size_t len)
{
	return ring_push(ring, heap_item, heap_item, len);
}

const char *agent_ring_peek(const struct agent_ring *ring,
	size_t *len_out)
{
	const struct agent_ring_slot *slot;

	if (ring->count == 0)
		return NULL;
	slot = &ring->slots[ring->head];
	if (len_out != NULL)
		*len_out = slot->len;
	return slot->heap != NULL ? slot->heap : slot->buf;
}

void agent_ring_pop(struct agent_ring *ring)
{
	struct agent_ring_slot *slot;

	if (ring->count == 0)
		return;
	slot = &ring->slots[ring->head];
	free(slot->heap);
	slot->heap = NULL;
	ring->head = (ring->head + 1) % AGENT_RING_CAP;
	ring->count--;
}
