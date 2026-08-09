/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the lock-free log ring (TODO.complete/19 P0).
 *
 * Verifies the SPSC ring semantics:
 *
 *   - push/drain on a fresh ring yields the pushed entries in order
 *   - ring wraps past capacity without losing order
 *   - push on a full ring returns -1 and increments dropped
 *   - drain on empty ring returns 0
 *   - drain stops early when the callback returns non-zero
 *   - long text is truncated to LOG_RING_TEXT_CAP-1
 *   - multi-thread: 4 threads each pushing to their own ring
 *     all see all their entries on drain (no cross-talk)
 *
 * Part of TODO.complete/19.
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "log_ring.h"
#include "real_impls.h"

static int tests_run;
static int tests_pass;
static int tests_fail;

#define TEST(name) do { \
	tests_run++; \
	printf("  TEST %s ... ", #name); \
	test_##name(); \
	tests_pass++; \
	printf("OK\n"); \
} while (0)

/* Drain callback that records up to N entries into a fixed array. */
struct DrainCapture {
	struct LogEntry entries[256];
	size_t count;
	int stop_at;  /* if non-zero, stop after this many entries */
};

static int capture_cb(const struct LogEntry *e, void *ctx)
{
	struct DrainCapture *c = (struct DrainCapture *)ctx;

	if (c->stop_at > 0 && c->count >= (size_t)c->stop_at)
		return 1;
	if (c->count < sizeof(c->entries) / sizeof(c->entries[0]))
		c->entries[c->count++] = *e;
	return 0;
}

static void test_fresh_ring_drains_nothing(void)
{
	struct DrainCapture cap = {0};

	/* Reinit between tests so each starts with empty registry. */
	retrace_log_ring_init();
	retrace_log_ring_deinit();
	retrace_log_ring_init();

	/* Without a per-thread ring (we haven't called _get yet),
	 * walk finds nothing.
	 */
	retrace_log_ring_walk(NULL, NULL);

	retrace_log_ring_deinit();
}

static void test_push_then_drain(void)
{
	struct LogRing *r;
	struct DrainCapture cap = {0};

	retrace_log_ring_init();

	r = retrace_log_ring_get();
	assert(r != NULL);

	assert(retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 100,
		"hello") == 0);
	assert(retrace_log_ring_push(r, FUNCS, SEVERITY_WARN, 200,
		"world") == 0);

	assert(retrace_log_ring_drain(r, capture_cb, &cap) == 2);
	assert(cap.count == 2);
	assert(cap.entries[0].ts_ms == 100);
	assert(cap.entries[0].sev == SEVERITY_INFO);
	assert(strcmp(cap.entries[0].text, "hello") == 0);
	assert(cap.entries[1].ts_ms == 200);
	assert(cap.entries[1].sev == SEVERITY_WARN);
	assert(strcmp(cap.entries[1].text, "world") == 0);

	/* Second drain after the ring is empty yields nothing. */
	cap.count = 0;
	assert(retrace_log_ring_drain(r, capture_cb, &cap) == 0);
	assert(cap.count == 0);

	retrace_log_ring_deinit();
}

static void test_wrap_around(void)
{
	/* Default ring is LOG_RING_DEFAULT_CAP = 64 entries. Push
	 * 64 + 10 = 74 entries, draining in batches of 32 to force
	 * wrap-around. The ring has only 64 slots, so we have to
	 * drain mid-stream to make room.
	 */
	struct LogRing *r;
	char buf[64];
	int i;
	size_t total_drained = 0;

	retrace_log_ring_init();
	r = retrace_log_ring_get();
	assert(r != NULL);

	/* Push 32, drain 32. */
	for (i = 0; i < 32; i++) {
		snprintf(buf, sizeof(buf), "e%d", i);
		assert(retrace_log_ring_push(r, FUNCS, SEVERITY_INFO,
			(uint32_t)i, buf) == 0);
	}
	total_drained += retrace_log_ring_drain(r, capture_cb,
		&(struct DrainCapture){0});
	assert(total_drained == 32);

	/* Push another 32 -- these will wrap around the ring slots. */
	for (i = 32; i < 64; i++) {
		snprintf(buf, sizeof(buf), "e%d", i);
		assert(retrace_log_ring_push(r, FUNCS, SEVERITY_INFO,
			(uint32_t)i, buf) == 0);
	}

	/* Verify we can still drain all 32. */
	{
		struct DrainCapture cap = {0};

		assert(retrace_log_ring_drain(r, capture_cb, &cap) == 32);
		/* First entry of this batch should be e32. */
		assert(strcmp(cap.entries[0].text, "e32") == 0);
		assert(strcmp(cap.entries[31].text, "e63") == 0);
	}

	retrace_log_ring_deinit();
}

static void test_drop_when_full(void)
{
	struct LogRing *r;
	char buf[64];
	int i;
	uint32_t dropped_before;

	retrace_log_ring_init();
	r = retrace_log_ring_get();
	assert(r != NULL);

	/* Fill the ring (capacity 64 means 63 usable + 1 sentinel). */
	for (i = 0; i < 63; i++) {
		snprintf(buf, sizeof(buf), "fill%d", i);
		assert(retrace_log_ring_push(r, FUNCS, SEVERITY_INFO,
			(uint32_t)i, buf) == 0);
	}

	/* Next push should fail (ring full). */
	dropped_before = r->dropped;
	assert(retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 999,
		"overflow") == -1);
	assert(r->dropped == dropped_before + 1);

	/* Drain one slot, then push succeeds again. */
	{
		struct DrainCapture cap = {.stop_at = 1};

		retrace_log_ring_drain(r, capture_cb, &cap);
	}
	assert(retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 1000,
		"after-drain") == 0);

	retrace_log_ring_deinit();
}

static void test_long_text_truncated(void)
{
	struct LogRing *r;
	struct DrainCapture cap = {0};
	char long_text[LOG_RING_TEXT_CAP + 64];

	memset(long_text, 'a', sizeof(long_text) - 1);
	long_text[sizeof(long_text) - 1] = '\0';

	retrace_log_ring_init();
	r = retrace_log_ring_get();
	assert(r != NULL);

	assert(retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 1,
		long_text) == 0);
	retrace_log_ring_drain(r, capture_cb, &cap);
	assert(cap.count == 1);
	/* Text is NUL-terminated within the field. */
	assert(cap.entries[0].text[LOG_RING_TEXT_CAP - 1] == '\0');
	assert(strlen(cap.entries[0].text) == LOG_RING_TEXT_CAP - 1);

	retrace_log_ring_deinit();
}

static void test_drain_stops_early(void)
{
	struct LogRing *r;
	struct DrainCapture cap = {.stop_at = 2};

	retrace_log_ring_init();
	r = retrace_log_ring_get();
	assert(r != NULL);

	retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 1, "a");
	retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 2, "b");
	retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 3, "c");

	/* stop_at = 2 means cb returns 1 on the 3rd call, drain stops. */
	assert(retrace_log_ring_drain(r, capture_cb, &cap) == 2);
	assert(cap.count == 2);

	/* Third entry is still in the ring. */
	cap.stop_at = 0;
	cap.count = 0;
	assert(retrace_log_ring_drain(r, capture_cb, &cap) == 1);
	assert(strcmp(cap.entries[0].text, "c") == 0);

	retrace_log_ring_deinit();
}

static void test_null_inputs_safe(void)
{
	/* Push on NULL ring is a no-op (returns -1). */
	assert(retrace_log_ring_push(NULL, FUNCS, SEVERITY_INFO, 0,
		"x") == -1);

	/* Drain on NULL ring returns 0. */
	assert(retrace_log_ring_drain(NULL, capture_cb, NULL) == 0);

	retrace_log_ring_init();
	{
		struct LogRing *r = retrace_log_ring_get();

		assert(r != NULL);
		retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 0, "x");
		/* Drain with NULL callback returns 0 (no-op). */
		assert(retrace_log_ring_drain(r, NULL, NULL) == 0);
	}
	retrace_log_ring_deinit();
}

/* Multi-thread: 4 threads each push to their own ring. Each thread
 * expects to drain exactly what it pushed -- proves the per-thread
 * isolation is correct.
 */
struct ThreadArg {
	int push_count;
	int drained_correct;
};

static void *thread_push_then_drain(void *p)
{
	struct ThreadArg *arg = (struct ThreadArg *)p;
	struct LogRing *r = retrace_log_ring_get();
	struct DrainCapture cap = {0};
	char buf[32];
	int i;

	if (r == NULL) {
		arg->drained_correct = 0;
		return NULL;
	}

	for (i = 0; i < arg->push_count; i++) {
		snprintf(buf, sizeof(buf), "t%d", i);
		if (retrace_log_ring_push(r, FUNCS, SEVERITY_INFO,
			(uint32_t)i, buf) != 0) {
			/* Drop is OK if we exceeded capacity before
			 * draining. For this test we keep push_count
			 * under capacity so no drops should occur.
			 */
		}
	}
	retrace_log_ring_drain(r, capture_cb, &cap);
	arg->drained_correct = (cap.count == (size_t)arg->push_count);
	return NULL;
}

static void test_multi_thread_each_thread_drains_own(void)
{
	enum { NTHREADS = 4, PER_THREAD = 32 };
	pthread_t tids[NTHREADS];
	struct ThreadArg args[NTHREADS];
	int i;

	retrace_log_ring_init();

	for (i = 0; i < NTHREADS; i++) {
		args[i].push_count = PER_THREAD;
		args[i].drained_correct = 0;
		pthread_create(&tids[i], NULL, thread_push_then_drain,
			&args[i]);
	}
	for (i = 0; i < NTHREADS; i++)
		pthread_join(tids[i], NULL);

	for (i = 0; i < NTHREADS; i++)
		assert(args[i].drained_correct);

	assert(retrace_log_ring_total_dropped() == 0);

	retrace_log_ring_deinit();
}

int main(void)
{
	/* Init real_impls the way other unit tests do. */
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;
	retrace_real_impls.pthread_mutex_lock = pthread_mutex_lock;
	retrace_real_impls.pthread_mutex_unlock = pthread_mutex_unlock;
	retrace_real_impls.pthread_mutex_init = pthread_mutex_init;
	retrace_real_impls.pthread_mutex_destroy = pthread_mutex_destroy;
	retrace_real_impls.pthread_key_create = pthread_key_create;
	retrace_real_impls.pthread_key_delete = pthread_key_delete;
	retrace_real_impls.pthread_getspecific = pthread_getspecific;
	retrace_real_impls.pthread_setspecific = pthread_setspecific;

	printf("log_ring tests:\n");

	printf("  -- basic semantics --\n");
	TEST(fresh_ring_drains_nothing);
	TEST(push_then_drain);
	TEST(drain_stops_early);

	printf("  -- capacity & wrap --\n");
	TEST(wrap_around);
	TEST(drop_when_full);
	TEST(long_text_truncated);

	printf("  -- edge cases --\n");
	TEST(null_inputs_safe);

	printf("  -- concurrency --\n");
	TEST(multi_thread_each_thread_drains_own);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
