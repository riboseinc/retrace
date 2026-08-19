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

#include "call_hash.h"

#include <stdatomic.h>
#include <stddef.h>

#include "real_impls.h"
#include "logger.h"

/*
 * FNV-1a 64-bit constants. Chosen for: simple, fast, well-mixed.
 * Not cryptographic; the goal is just "different sequences hash
 * differently" with low collision probability over realistic
 * fuzz runs (< 2^32 calls).
 */
#define FNV_OFFSET ((uint64_t)0xcbf29ce484222325ULL)
#define FNV_PRIME  ((uint64_t)0x100000001b3ULL)

/*
 * Exported global updated on every call_hash_observe. Read by
 * the libFuzzer custom mutator (TODO.complete/24 P1).
 */
uint64_t retrace_call_hash_last;

struct ThreadHash {
	uint64_t hash;

	int observed;  /* 1 once observe() has been called at least once */
};

struct HashNode {
	struct ThreadHash h;

	struct HashNode *next;
};

static struct {
	int enabled;

	rc_mutex_t mtx;

	struct HashNode *head;
} g_state;

static rc_tss_t g_thread_key;

/*
 * pthread_key destructor. The thread is exiting; its hash is
 * still in the registry for the deinit walk to surface.
 */
static void hash_key_destructor(void *p)
{
	(void)p;
	retrace_real_impls.rc_tss_set(g_thread_key, NULL);
}

int retrace_call_hash_init(void)
{
	char *env;
	int rc;

	env = retrace_real_impls.getenv("RETRACE_CALL_HASH");
	g_state.enabled = (env != NULL && env[0] != '\0' &&
		env[0] != '0');

	rc = retrace_real_impls.rc_mutex_init(&g_state.mtx);
	if (rc != 0) {
		log_err("call_hash: pthread_mutex_init failed: %d", rc);
		return -1;
	}

	rc = retrace_real_impls.rc_tss_create(&g_thread_key,
		hash_key_destructor);
	if (rc != 0) {
		log_err("call_hash: pthread_key_create failed: %d", rc);
		retrace_real_impls.rc_mutex_destroy(&g_state.mtx);
		return -1;
	}

	g_state.head = NULL;
	return 0;
}

int retrace_call_hash_enabled(void)
{
	return g_state.enabled;
}

static struct ThreadHash *thread_hash_get(void)
{
	struct HashNode *node = (struct HashNode *)
		retrace_real_impls.rc_tss_get(g_thread_key);

	if (node != NULL)
		return &node->h;

	node = (struct HashNode *)retrace_real_impls.malloc(sizeof(*node));
	if (node == NULL)
		return NULL;

	node->h.hash = FNV_OFFSET;
	node->h.observed = 0;
	node->next = NULL;

	retrace_real_impls.rc_mutex_lock(&g_state.mtx);
	node->next = g_state.head;
	g_state.head = node;
	retrace_real_impls.rc_mutex_unlock(&g_state.mtx);

	if (retrace_real_impls.rc_tss_set(g_thread_key, node) != 0) {
		/* Rare. The hash is in the registry but the caller
		 * can't update it. Subsequent observe() calls will
		 * allocate a new node -- wasteful but safe.
		 */
		return NULL;
	}
	return &node->h;
}

void retrace_call_hash_observe(const char *func_name, int arg_count)
{
	struct ThreadHash *th;
	const unsigned char *p;
	uint64_t h;

	if (!g_state.enabled || func_name == NULL)
		return;

	th = thread_hash_get();
	if (th == NULL)
		return;

	h = th->hash;
	for (p = (const unsigned char *)func_name; *p != '\0'; p++) {
		h ^= (uint64_t)*p;
		h *= FNV_PRIME;
	}
	h ^= (uint64_t)arg_count;
	h *= FNV_PRIME;

	th->hash = h;
	th->observed = 1;

	/*
	 * Publish the latest hash to the exported global so a
	 * libFuzzer custom mutator can read it between iterations
	 * (TODO.complete/24 P1). __sync_lock_test_and_set provides
	 * a full memory barrier.
	 */
	__sync_lock_test_and_set(&retrace_call_hash_last, h);
}

uint64_t retrace_call_hash_get(void)
{
	struct ThreadHash *th = thread_hash_get();

	if (th == NULL || !th->observed)
		return 0;
	return th->hash;
}

void retrace_call_hash_walk(retrace_call_hash_walk_cb cb, void *ctx)
{
	struct HashNode *node;

	if (cb == NULL)
		return;

	/* Same lock-free walk pattern as log_ring: appends prepend
	 * under the mutex; walk reads head without taking it. Safe
	 * because we either see the new node or we don't, never a
	 * partial node.
	 */
	for (node = g_state.head; node != NULL; node = node->next) {
		if (node->h.observed)
			cb(node->h.hash, ctx);
	}
}

void retrace_call_hash_deinit(void)
{
	struct HashNode *node = g_state.head;

	while (node != NULL) {
		struct HashNode *next = node->next;

		retrace_real_impls.free(node);
		node = next;
	}

	g_state.head = NULL;
	retrace_real_impls.rc_mutex_destroy(&g_state.mtx);
}
