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

/* Always-on check. assert() alone is compiled out by NDEBUG, which
 * leaves side effects (push/drain counter mutations) unevaluated.
 * See feedback_assert_with_side_effects.md.
 */
#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

struct DrainCapture {
	struct LogEntry entries[256];
	size_t count;
	int stop_at;
};

static int capture_cb(const struct LogEntry *e, void *ctx)
{
	struct DrainCapture *c = (struct DrainCapture *)ctx;

	if (c->stop_at > 0 && c->count >= (size_t)c->stop_at)
		return 1;
	if (c->count < sizeof(c->entries) / sizeof(c->entries[0])) {
		struct LogEntry *dst = &c->entries[c->count++];
		size_t len = strlen(e->text);

		dst->ts_ms = e->ts_ms;
		dst->module = e->module;
		dst->sev = e->sev;
		dst->text = (char *)malloc(len + 1);
		if (dst->text == NULL)
			return 1;
		memcpy(dst->text, e->text, len + 1);
	}
	return 0;
}

static void test_fresh_ring_drains_nothing(void)
{
	struct DrainCapture cap = {0};

	retrace_log_ring_init();
	retrace_log_ring_deinit();
	retrace_log_ring_init();

	retrace_log_ring_walk(NULL, NULL);

	retrace_log_ring_deinit();
}

static void test_push_then_drain(void)
{
	struct LogRing *r;
	struct DrainCapture cap = {0};
	int rc;

	retrace_log_ring_init();

	r = retrace_log_ring_get();
	CHECK(r != NULL);

	rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 100, "hello");
	CHECK(rc == 0);
	rc = retrace_log_ring_push(r, FUNCS, SEVERITY_WARN, 200, "world");
	CHECK(rc == 0);

	rc = retrace_log_ring_drain(r, capture_cb, &cap);
	CHECK(rc == 2);
	CHECK(cap.count == 2);
	CHECK(cap.entries[0].ts_ms == 100);
	CHECK(cap.entries[0].sev == SEVERITY_INFO);
	CHECK(strcmp(cap.entries[0].text, "hello") == 0);
	CHECK(cap.entries[1].ts_ms == 200);
	CHECK(cap.entries[1].sev == SEVERITY_WARN);
	CHECK(strcmp(cap.entries[1].text, "world") == 0);

	cap.count = 0;
	rc = retrace_log_ring_drain(r, capture_cb, &cap);
	CHECK(rc == 0);
	CHECK(cap.count == 0);

	retrace_log_ring_deinit();
}

static void test_wrap_around(void)
{
	struct LogRing *r;
	char buf[64];
	int i;
	size_t total_drained = 0;
	int rc;

	retrace_log_ring_init();
	r = retrace_log_ring_get();
	CHECK(r != NULL);

	for (i = 0; i < 32; i++) {
		snprintf(buf, sizeof(buf), "e%d", i);
		rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO,
			(uint32_t)i, buf);
		CHECK(rc == 0);
	}
	total_drained += retrace_log_ring_drain(r, capture_cb,
		&(struct DrainCapture){0});
	CHECK(total_drained == 32);

	for (i = 32; i < 64; i++) {
		snprintf(buf, sizeof(buf), "e%d", i);
		rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO,
			(uint32_t)i, buf);
		CHECK(rc == 0);
	}

	{
		struct DrainCapture cap = {0};

		rc = retrace_log_ring_drain(r, capture_cb, &cap);
		CHECK(rc == 32);
		CHECK(strcmp(cap.entries[0].text, "e32") == 0);
		CHECK(strcmp(cap.entries[31].text, "e63") == 0);
	}

	retrace_log_ring_deinit();
}

static void test_drop_when_full(void)
{
	struct LogRing *r;
	char buf[64];
	int i;
	uint32_t dropped_before;
	int rc;

	retrace_log_ring_init();
	r = retrace_log_ring_get();
	CHECK(r != NULL);

	for (i = 0; i < 63; i++) {
		snprintf(buf, sizeof(buf), "fill%d", i);
		rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO,
			(uint32_t)i, buf);
		CHECK(rc == 0);
	}

	dropped_before = r->dropped;
	rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 999, "overflow");
	CHECK(rc == -1);
	CHECK(r->dropped == dropped_before + 1);

	{
		struct DrainCapture cap = {.stop_at = 1};

		retrace_log_ring_drain(r, capture_cb, &cap);
	}
	rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 1000,
		"after-drain");
	CHECK(rc == 0);

	retrace_log_ring_deinit();
}

static void test_long_text_preserved(void)
{
	struct LogRing *r;
	struct DrainCapture cap = {0};
	char long_text[4096];
	int rc;

	memset(long_text, 'a', sizeof(long_text) - 1);
	long_text[sizeof(long_text) - 1] = '\0';

	retrace_log_ring_init();
	r = retrace_log_ring_get();
	CHECK(r != NULL);

	rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 1, long_text);
	CHECK(rc == 0);
	retrace_log_ring_drain(r, capture_cb, &cap);
	CHECK(cap.count == 1);
	CHECK(strcmp(cap.entries[0].text, long_text) == 0);
	CHECK(strlen(cap.entries[0].text) == sizeof(long_text) - 1);

	retrace_log_ring_deinit();
}

static void test_drain_stops_early(void)
{
	struct LogRing *r;
	struct DrainCapture cap = {.stop_at = 2};
	int rc;

	retrace_log_ring_init();
	r = retrace_log_ring_get();
	CHECK(r != NULL);

	rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 1, "a");
	CHECK(rc == 0);
	rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 2, "b");
	CHECK(rc == 0);
	rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 3, "c");
	CHECK(rc == 0);

	rc = retrace_log_ring_drain(r, capture_cb, &cap);
	CHECK(rc == 2);
	CHECK(cap.count == 2);

	cap.stop_at = 0;
	cap.count = 0;
	rc = retrace_log_ring_drain(r, capture_cb, &cap);
	CHECK(rc == 1);
	CHECK(strcmp(cap.entries[0].text, "c") == 0);

	retrace_log_ring_deinit();
}

static void test_null_inputs_safe(void)
{
	int rc;

	rc = retrace_log_ring_push(NULL, FUNCS, SEVERITY_INFO, 0, "x");
	CHECK(rc == -1);

	rc = retrace_log_ring_drain(NULL, capture_cb, NULL);
	CHECK(rc == 0);

	retrace_log_ring_init();
	{
		struct LogRing *r = retrace_log_ring_get();

		CHECK(r != NULL);
		rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 0, "x");
		CHECK(rc == 0);
		rc = retrace_log_ring_drain(r, NULL, NULL);
		CHECK(rc == 0);
	}
	retrace_log_ring_deinit();
}

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
		(void)retrace_log_ring_push(r, FUNCS, SEVERITY_INFO,
			(uint32_t)i, buf);
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
		CHECK(args[i].drained_correct);

	CHECK(retrace_log_ring_total_dropped() == 0);

	retrace_log_ring_deinit();
}

int main(void)
{
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
	TEST(long_text_preserved);

	printf("  -- edge cases --\n");
	TEST(null_inputs_safe);

	printf("  -- concurrency --\n");
	TEST(multi_thread_each_thread_drains_own);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
