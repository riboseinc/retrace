/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the background log flusher (TODO.complete/19 PR B).
 *
 * Verifies:
 *
 *   - init spawns a thread, stop joins it
 *   - entries pushed after init get drained by the flusher
 *   - the final drain on stop captures in-flight entries
 *   - drain_all without a thread works as a one-shot flush
 *   - per-thread rings from multiple threads all get drained
 *   - NULL callback to init is rejected
 *   - calling stop without init is a no-op
 *
 * Part of TODO.complete/19.
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "log_ring.h"
#include "log_flusher.h"
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

/* Always-on check. assert() alone is compiled out by NDEBUG.
 * See feedback_assert_with_side_effects.md.
 */
#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

struct EmitCapture {
	struct LogEntry entries[512];
	size_t count;
};

static void emit_capture_reset(struct EmitCapture *c)
{
	c->count = 0;
}

static int emit_capture(const struct LogEntry *e, void *p)
{
	struct EmitCapture *c = (struct EmitCapture *)p;

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

static void test_drain_all_oneshot(void)
{
	struct LogRing *r;
	struct EmitCapture cap;
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

	emit_capture_reset(&cap);
	rc = retrace_log_flusher_drain_all(emit_capture, &cap);
	CHECK(rc == 3);
	CHECK(cap.count == 3);
	CHECK(strcmp(cap.entries[0].text, "a") == 0);
	CHECK(strcmp(cap.entries[2].text, "c") == 0);

	retrace_log_ring_deinit();
}

static void test_init_stop_lifecycle(void)
{
	struct EmitCapture cap;
	int rc;

	retrace_log_ring_init();
	emit_capture_reset(&cap);

	rc = retrace_log_flusher_init(emit_capture, &cap);
	CHECK(rc == 0);

	rc = retrace_log_flusher_init(emit_capture, &cap);
	CHECK(rc == -1);

	retrace_log_flusher_stop();
	retrace_log_flusher_stop();

	retrace_log_ring_deinit();
}

static void test_flusher_drains_pushed_entries(void)
{
	struct LogRing *r;
	struct EmitCapture cap;
	int rc;

	retrace_log_ring_init();
	emit_capture_reset(&cap);

	rc = retrace_log_flusher_init(emit_capture, &cap);
	CHECK(rc == 0);

	r = retrace_log_ring_get();
	CHECK(r != NULL);
	rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 100, "alpha");
	CHECK(rc == 0);

	{
		struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000};
		int tries = 50;

		while (cap.count == 0 && tries-- > 0)
			nanosleep(&ts, NULL);
	}

	retrace_log_flusher_stop();

	CHECK(cap.count >= 1);
	CHECK(strcmp(cap.entries[0].text, "alpha") == 0);

	retrace_log_ring_deinit();
}

static void test_stop_does_final_drain(void)
{
	struct LogRing *r;
	struct EmitCapture cap;
	int rc;

	retrace_log_ring_init();
	emit_capture_reset(&cap);

	rc = retrace_log_flusher_init(emit_capture, &cap);
	CHECK(rc == 0);

	r = retrace_log_ring_get();
	rc = retrace_log_ring_push(r, FUNCS, SEVERITY_INFO, 1, "final-drain");
	CHECK(rc == 0);

	retrace_log_flusher_stop();

	CHECK(cap.count >= 1);
	CHECK(strcmp(cap.entries[cap.count - 1].text, "final-drain") == 0);

	retrace_log_ring_deinit();
}

static void test_null_cb_rejected(void)
{
	int rc;

	retrace_log_ring_init();
	rc = retrace_log_flusher_init(NULL, NULL);
	CHECK(rc == -1);
	retrace_log_ring_deinit();
}

static void test_stop_without_init_noop(void)
{
	retrace_log_ring_init();
	retrace_log_flusher_stop();
	retrace_log_ring_deinit();
}

struct StressArg {
	int n;
};

static void *stress_pusher(void *p)
{
	struct StressArg *a = (struct StressArg *)p;
	struct LogRing *r = retrace_log_ring_get();
	char buf[32];
	int i;

	if (r == NULL)
		return NULL;
	for (i = 0; i < a->n; i++) {
		snprintf(buf, sizeof(buf), "s%d", i);
		(void)retrace_log_ring_push(r, FUNCS, SEVERITY_INFO,
			(uint32_t)i, buf);
	}
	return NULL;
}

static void test_multi_thread_all_drained(void)
{
	enum { NTHREADS = 4, PER_THREAD = 32 };
	pthread_t tids[NTHREADS];
	struct StressArg args[NTHREADS];
	struct EmitCapture cap;
	int i;
	int rc;

	retrace_log_ring_init();
	emit_capture_reset(&cap);

	rc = retrace_log_flusher_init(emit_capture, &cap);
	CHECK(rc == 0);

	for (i = 0; i < NTHREADS; i++) {
		args[i].n = PER_THREAD;
		pthread_create(&tids[i], NULL, stress_pusher, &args[i]);
	}
	for (i = 0; i < NTHREADS; i++)
		pthread_join(tids[i], NULL);

	retrace_log_flusher_stop();

	CHECK(cap.count == (size_t)(NTHREADS * PER_THREAD));
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
	retrace_real_impls.pthread_create = pthread_create;
	retrace_real_impls.pthread_join = pthread_join;

	printf("log_flusher tests:\n");

	printf("  -- one-shot drain --\n");
	TEST(drain_all_oneshot);

	printf("  -- lifecycle --\n");
	TEST(init_stop_lifecycle);
	TEST(null_cb_rejected);
	TEST(stop_without_init_noop);

	printf("  -- thread-driven drain --\n");
	TEST(flusher_drains_pushed_entries);
	TEST(stop_does_final_drain);

	printf("  -- multi-thread stress --\n");
	TEST(multi_thread_all_drained);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
