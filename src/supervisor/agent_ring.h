/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_SUPERVISOR_AGENT_RING_H_
#define RETRACE_SUPERVISOR_AGENT_RING_H_

#include <stddef.h>
#include <stdint.h>

/*
 * The agent's bounded event ring -- the ONE queue both platform
 * halves drain (they each carried a copy; they drifted). PURE:
 * the caller holds the transport's lock around every call, and
 * condition-variable wakes stay with the caller -- the ring
 * knows nothing of pthreads or critical sections.
 *
 * Storage policy, one rule: items up to AGENT_RING_INLINE copy
 * into the slot; larger items heap-own their bytes (the heap
 * fallback the Windows half retrofitted, now both halves').
 * Every refusal is COUNTED in ring->dropped -- losses are
 * reported, never silent; the doctrine lives here once.
 *
 * peek/pop split: the consumer sends the peeked view first and
 * releases the slot after -- pop frees the heap when present.
 */

#define AGENT_RING_CAP 256
#define AGENT_RING_INLINE 768

struct agent_ring_slot {
	char *heap;		/* oversize items own their bytes */
	size_t len;
	char buf[AGENT_RING_INLINE];
};

struct agent_ring {
	struct agent_ring_slot slots[AGENT_RING_CAP];
	size_t head;		/* next to release */
	size_t count;
	uint64_t dropped;
};

/* zero the ring (before first use; nothing allocated yet) */
void agent_ring_init(struct agent_ring *ring);

/* queue a copy of item (0 queued, -1 dropped+counted) */
int agent_ring_push_copy(struct agent_ring *ring, const char *item,
	size_t len);

/*
 * queue a heap string the ring owns on success (and frees on
 * refusal -- the drop is still counted). len includes the
 * terminating NUL the heap form carries.
 */
int agent_ring_push_heap(struct agent_ring *ring, char *heap_item,
	size_t len);

/* view the head (NULL when empty); *len_out optional */
const char *agent_ring_peek(const struct agent_ring *ring,
	size_t *len_out);

/* release the head (frees the heap when present); no-op empty */
void agent_ring_pop(struct agent_ring *ring);

#endif /* RETRACE_SUPERVISOR_AGENT_RING_H_ */
