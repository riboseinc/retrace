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

#include "log_ring.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>

/*
 * Note: deliberately does NOT include <string.h> or <stdlib.h>.
 * On macOS with _USE_FORTIFY_LEVEL > 0 (the default), those headers
 * macro-substitute memcpy/memset/strdup/etc., which would rewrite
 * `retrace_real_impls.memcpy(...)` to
 * `retrace_real_impls.__builtin___memcpy_chk(...)` and break the
 * call. The function pointer types we need are already declared via
 * real_impls.h (transitively via <stdio.h>/<stddef.h>).
 *
 * Other consumers (sockaddr_inspect.c, caller_cache.c) avoid this
 * by simply not including <string.h>. We follow the same pattern.
 */

#include "real_impls.h"
#include "logger.h"

/*
 * Global registry of every ring ever allocated. Walked by the
 * flusher. Rings are never removed during normal operation -- they
 * stay until deinit -- because a thread that exited may still have
 * undrained entries that the flusher needs to surface.
 *
 * Implementation: singly-linked list with a pthread_mutex to guard
 * the append. The mutex is ONLY taken on ring creation (once per
 * thread lifetime), never on push -- the push path stays lock-free.
 */
struct RingNode {
	struct LogRing *ring;
	struct RingNode *next;
};

static struct {
	pthread_mutex_t mtx;
	struct RingNode *head;
	uint64_t total_dropped;
} g_registry;

static pthread_key_t g_ring_key;

/*
 * Allocate a new ring with the given capacity (must be power-of-2).
 * Returns NULL on OOM or invalid capacity.
 */
static struct LogRing *ring_alloc(uint32_t capacity)
{
	struct LogRing *r;
	uint32_t cap;

	if (capacity == 0 || (capacity & (capacity - 1)) != 0)
		return NULL;

	r = (struct LogRing *)retrace_real_impls.malloc(sizeof(*r));
	if (r == NULL)
		return NULL;

	cap = capacity;
	r->entries = (struct LogEntry *)retrace_real_impls.malloc(
		(size_t)cap * sizeof(struct LogEntry));
	if (r->entries == NULL) {
		retrace_real_impls.free(r);
		return NULL;
	}

	r->mask = cap - 1;
	r->dropped = 0;
	atomic_store_explicit(&r->head, 0, memory_order_relaxed);
	atomic_store_explicit(&r->tail, 0, memory_order_relaxed);
	return r;
}

static void ring_free(struct LogRing *r)
{
	if (r == NULL)
		return;
	/* Free any un-drained text copies so we don't leak on deinit. */
	if (r->entries != NULL) {
		uint32_t t = atomic_load_explicit(&r->tail,
			memory_order_relaxed);
		uint32_t h = atomic_load_explicit(&r->head,
			memory_order_relaxed);

		while (t != h) {
			retrace_real_impls.free(r->entries[t].text);
			r->entries[t].text = NULL;
			t = (t + 1) & r->mask;
		}
	}
	retrace_real_impls.free(r->entries);
	retrace_real_impls.free(r);
}

/* Registry append. Called once per thread on first push. */
static void registry_add(struct LogRing *r)
{
	struct RingNode *node = (struct RingNode *)
		retrace_real_impls.malloc(sizeof(*node));

	if (node == NULL) {
		/* OOM on registry -- free the ring and pretend it never
		 * existed. The caller's push will see NULL and skip.
		 */
		ring_free(r);
		return;
	}
	node->ring = r;

	retrace_real_impls.pthread_mutex_lock(&g_registry.mtx);
	node->next = g_registry.head;
	g_registry.head = node;
	retrace_real_impls.pthread_mutex_unlock(&g_registry.mtx);
}

/*
 * pthread_key destructor. The thread is exiting; its ring is
 * now orphaned. We don't free it -- the flusher still needs to
 * walk it for a final drain. The ring is freed at deinit.
 *
 * Sets the per-thread key to NULL so a subsequent push from this
 * thread (rare but possible during thread teardown) doesn't find
 * a dangling pointer.
 */
static void ring_key_destructor(void *p)
{
	(void)p;
	retrace_real_impls.pthread_setspecific(g_ring_key, NULL);
}

static uint32_t g_ring_cap;

/*
 * Parse RETRACE_LOGGER_RING_CAP once at module init. Accepted:
 * power-of-two values in [LOG_RING_CAP_MIN, LOG_RING_CAP_MAX].
 * Anything else (unset, malformed, out of range) falls back to
 * LOG_RING_DEFAULT_CAP. Returns the effective capacity.
 */
static uint32_t ring_configured_cap(void)
{
	const char *env = retrace_real_impls.getenv
		? retrace_real_impls.getenv("RETRACE_LOGGER_RING_CAP")
		: NULL;
	long v;

	if (env == NULL || *env == '\0')
		return LOG_RING_DEFAULT_CAP;

	if (retrace_real_impls.atoi == NULL)
		return LOG_RING_DEFAULT_CAP;
	v = (long)retrace_real_impls.atoi(env);
	if (v < (long)LOG_RING_CAP_MIN || v > (long)LOG_RING_CAP_MAX)
		return LOG_RING_DEFAULT_CAP;
	if ((v & (v - 1)) != 0)
		return LOG_RING_DEFAULT_CAP;
	return (uint32_t)v;
}

int retrace_log_ring_init(void)
{
	int rc;

	g_ring_cap = ring_configured_cap();

	rc = retrace_real_impls.pthread_mutex_init(&g_registry.mtx, NULL);
	if (rc != 0) {
		log_err("log_ring: pthread_mutex_init failed: %d", rc);
		return -1;
	}

	rc = retrace_real_impls.pthread_key_create(&g_ring_key,
		ring_key_destructor);
	if (rc != 0) {
		log_err("log_ring: pthread_key_create failed: %d", rc);
		retrace_real_impls.pthread_mutex_destroy(&g_registry.mtx);
		return -1;
	}

	g_registry.head = NULL;
	g_registry.total_dropped = 0;
	return 0;
}

void retrace_log_ring_deinit(void)
{
	struct RingNode *node = g_registry.head;

	while (node != NULL) {
		struct RingNode *next = node->next;

		if (node->ring != NULL)
			g_registry.total_dropped += node->ring->dropped;
		ring_free(node->ring);
		retrace_real_impls.free(node);
		node = next;
	}

	g_registry.head = NULL;
	retrace_real_impls.pthread_mutex_destroy(&g_registry.mtx);
}

struct LogRing *retrace_log_ring_get(void)
{
	struct LogRing *r = (struct LogRing *)
		retrace_real_impls.pthread_getspecific(g_ring_key);

	if (r != NULL)
		return r;

	r = ring_alloc(g_ring_cap ? g_ring_cap : LOG_RING_DEFAULT_CAP);
	if (r == NULL)
		return NULL;

	registry_add(r);

	if (retrace_real_impls.pthread_setspecific(g_ring_key, r) != 0) {
		/* Rare. Ring is in the registry; the flusher will
		 * drain it but the caller can't push. Let the caller
		 * see NULL and skip the log.
		 */
		return NULL;
	}
	return r;
}

int retrace_log_ring_push(struct LogRing *r, uint8_t module,
			  uint8_t sev, uint32_t ts_ms, const char *text)
{
	uint32_t h;
	uint32_t next;
	uint32_t t;
	size_t text_len;
	char *text_copy;
	struct LogEntry *e;

	if (r == NULL || text == NULL)
		return -1;

	/* Heap-copy the text up front so the ring entry stays small
	 * (16 bytes) and message length is unbounded. The copy is
	 * freed by drain() after the consumer callback returns.
	 *
	 * We allocate BEFORE the ring-full check so that on full-ring
	 * we can free immediately and the producer sees a clean -1.
	 * Alternative (allocate after check) saves a malloc on drop
	 * but creates a TOCTOU: between check and store, the consumer
	 * could drain, making the push succeed with a stale text.
	 */
	text_len = retrace_real_impls.strlen(text);
	text_copy = (char *)retrace_real_impls.malloc(text_len + 1);
	if (text_copy == NULL)
		return -1;
	retrace_real_impls.memcpy(text_copy, text, text_len);
	text_copy[text_len] = '\0';

	h = atomic_load_explicit(&r->head, memory_order_relaxed);
	next = (h + 1) & r->mask;
	t = atomic_load_explicit(&r->tail, memory_order_acquire);

	if (next == t) {
		/* Ring full. Drop the copy and count. */
		retrace_real_impls.free(text_copy);
		r->dropped++;
		return -1;
	}

	e = &r->entries[h];
	e->ts_ms = ts_ms;
	e->module = module;
	e->sev = sev;
	e->_reserved = 0;
	e->text = text_copy;

	atomic_store_explicit(&r->head, next, memory_order_release);
	return 0;
}

size_t retrace_log_ring_drain(struct LogRing *r,
			      retrace_log_ring_drain_cb cb, void *ctx)
{
	uint32_t t;
	uint32_t h;
	size_t count = 0;

	if (r == NULL || cb == NULL)
		return 0;

	t = atomic_load_explicit(&r->tail, memory_order_relaxed);
	h = atomic_load_explicit(&r->head, memory_order_acquire);

	while (t != h) {
		struct LogEntry *e = &r->entries[t];

		if (cb(e, ctx) != 0)
			break;

		/* Free the heap-allocated text now that the consumer
		 * has seen it. The next push to this slot will write
		 * a fresh pointer.
		 */
		retrace_real_impls.free(e->text);
		e->text = NULL;
		t = (t + 1) & r->mask;
		count++;
	}

	atomic_store_explicit(&r->tail, t, memory_order_release);
	return count;
}

struct WalkCtx {
	retrace_log_ring_walk_cb cb;
	void *user_ctx;
};

/* Walk callback adapter -- we don't actually need to adapt since
 * the registry walk yields the same struct LogRing*. Helper inline
 * keeps the deinit path readable.
 */
void retrace_log_ring_walk(retrace_log_ring_walk_cb cb, void *ctx)
{
	struct RingNode *node;

	if (cb == NULL)
		return;

	/* Walk doesn't take the mutex: appends only prepend, so a
	 * concurrent registry_add is safe (we either see the new
	 * node or we don't, but we never see a partial node).
	 */
	for (node = g_registry.head; node != NULL; node = node->next) {
		if (node->ring != NULL)
			cb(node->ring, ctx);
	}
}

uint64_t retrace_log_ring_total_dropped(void)
{
	struct RingNode *node;
	uint64_t total = 0;

	for (node = g_registry.head; node != NULL; node = node->next) {
		if (node->ring != NULL)
			total += node->ring->dropped;
	}
	return total;
}
