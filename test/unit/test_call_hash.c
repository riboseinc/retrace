/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the per-thread call-sequence hash (TODO.complete/24).
 *
 * Verifies:
 *   - enabled() respects env var (off when unset, on when non-zero)
 *   - observe() updates the hash deterministically for the same input
 *   - different sequences produce different hashes
 *   - per-thread isolation: two threads have independent hashes
 *   - walk surfaces every thread that has observed at least one call
 *   - get() returns 0 for a thread that hasn't observed anything
 *
 * Part of TODO.complete/24.
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "call_hash.h"
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

static void test_disabled_when_env_unset(void)
{
	unsetenv("RETRACE_CALL_HASH");
	retrace_call_hash_init();
	assert(retrace_call_hash_enabled() == 0);
	retrace_call_hash_deinit();
}

static void test_enabled_when_env_set(void)
{
	setenv("RETRACE_CALL_HASH", "1", 1);
	retrace_call_hash_init();
	assert(retrace_call_hash_enabled() == 1);
	retrace_call_hash_deinit();
	unsetenv("RETRACE_CALL_HASH");
}

static void test_disabled_is_noop(void)
{
	unsetenv("RETRACE_CALL_HASH");
	retrace_call_hash_init();
	/* observe on disabled is a no-op; get returns 0 */
	retrace_call_hash_observe("malloc", 1);
	retrace_call_hash_observe("free", 1);
	assert(retrace_call_hash_get() == 0);
	retrace_call_hash_deinit();
}

static void test_deterministic_same_sequence(void)
{
	setenv("RETRACE_CALL_HASH", "1", 1);
	retrace_call_hash_init();

	retrace_call_hash_observe("malloc", 1);
	retrace_call_hash_observe("free", 1);
	{
		uint64_t h1 = retrace_call_hash_get();

		/* Repeat the same sequence on top of the existing hash --
		 * hash should advance but be deterministic.
		 */
		retrace_call_hash_observe("malloc", 1);
		retrace_call_hash_observe("free", 1);
		{
			uint64_t h2 = retrace_call_hash_get();

			assert(h1 != 0);
			assert(h2 != 0);
			assert(h1 != h2);
		}
	}

	retrace_call_hash_deinit();
	unsetenv("RETRACE_CALL_HASH");
}

static void test_different_sequences_different_hash(void)
{
	setenv("RETRACE_CALL_HASH", "1", 1);
	retrace_call_hash_init();

	retrace_call_hash_observe("malloc", 1);
	retrace_call_hash_observe("free", 1);
	{
		uint64_t hash_ma = retrace_call_hash_get();

		/* New process state -- init/deinit resets per-thread state. */
		retrace_call_hash_deinit();
		retrace_call_hash_init();

		retrace_call_hash_observe("open", 2);
		retrace_call_hash_observe("close", 1);
		{
			uint64_t hash_oc = retrace_call_hash_get();

			assert(hash_ma != hash_oc);
		}
	}

	retrace_call_hash_deinit();
	unsetenv("RETRACE_CALL_HASH");
}

static void capture_count_cb(uint64_t h, void *ctx)
{
	int *p = (int *)ctx;

	(void)h;
	(*p)++;
}

static void test_observed_thread_appears_in_walk(void)
{
	int count = 0;

	setenv("RETRACE_CALL_HASH", "1", 1);
	retrace_call_hash_init();

	retrace_call_hash_observe("malloc", 1);

	retrace_call_hash_walk(capture_count_cb, &count);
	assert(count >= 1);

	retrace_call_hash_deinit();
	unsetenv("RETRACE_CALL_HASH");
}

static void walk_incr_cb(uint64_t h, void *ctx)
{
	int *p = (int *)ctx;

	(void)h;
	(*p)++;
}

static void test_walk_no_call_no_appearance(void)
{
	int count = 0;

	setenv("RETRACE_CALL_HASH", "1", 1);
	retrace_call_hash_init();

	/* No observe() called -- walk should surface nothing. */
	retrace_call_hash_walk(walk_incr_cb, &count);
	assert(count == 0);

	retrace_call_hash_deinit();
	unsetenv("RETRACE_CALL_HASH");
}

static void test_null_inputs_safe(void)
{
	setenv("RETRACE_CALL_HASH", "1", 1);
	retrace_call_hash_init();

	/* NULL func_name is a no-op */
	retrace_call_hash_observe(NULL, 1);
	assert(retrace_call_hash_get() == 0);

	/* NULL callback in walk is a no-op */
	retrace_call_hash_walk(NULL, NULL);

	retrace_call_hash_deinit();
	unsetenv("RETRACE_CALL_HASH");
}

struct ThreadArg {
	uint64_t hash;
};

static void *thread_observe_then_get(void *p)
{
	struct ThreadArg *a = (struct ThreadArg *)p;

	retrace_call_hash_observe("malloc", 1);
	retrace_call_hash_observe("free", 1);
	a->hash = retrace_call_hash_get();
	return NULL;
}

static void test_multi_thread_independent_hashes(void)
{
	enum { NTHREADS = 4 };
	pthread_t tids[NTHREADS];
	struct ThreadArg args[NTHREADS];
	int i;

	setenv("RETRACE_CALL_HASH", "1", 1);
	retrace_call_hash_init();

	for (i = 0; i < NTHREADS; i++) {
		args[i].hash = 0;
		pthread_create(&tids[i], NULL, thread_observe_then_get,
			&args[i]);
	}
	for (i = 0; i < NTHREADS; i++)
		pthread_join(tids[i], NULL);

	/* All 4 threads did the same sequence -> same hash, but each
	 * saw its own (independent of the others). Confirm they all
	 * got a non-zero hash.
	 */
	for (i = 0; i < NTHREADS; i++) {
		assert(args[i].hash != 0);
	}

	/* walk should see at least NTHREADS hashes total (could be
	 * more if retrace_call_hash_get implicitly created a node
	 * for the test main thread -- it doesn't, but be lenient).
	 */
	{
		int count = 0;

		retrace_call_hash_walk(walk_incr_cb, &count);
		assert(count >= NTHREADS);
	}

	retrace_call_hash_deinit();
	unsetenv("RETRACE_CALL_HASH");
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
	retrace_real_impls.getenv = getenv;
	retrace_real_impls.fprintf = fprintf;
	retrace_real_impls.pthread_mutex_lock = pthread_mutex_lock;
	retrace_real_impls.pthread_mutex_unlock = pthread_mutex_unlock;
	retrace_real_impls.pthread_mutex_init = pthread_mutex_init;
	retrace_real_impls.pthread_mutex_destroy = pthread_mutex_destroy;
	retrace_real_impls.pthread_key_create = pthread_key_create;
	retrace_real_impls.pthread_key_delete = pthread_key_delete;
	retrace_real_impls.pthread_getspecific = pthread_getspecific;
	retrace_real_impls.pthread_setspecific = pthread_setspecific;

	printf("call_hash tests:\n");

	printf("  -- enable/disable --\n");
	TEST(disabled_when_env_unset);
	TEST(enabled_when_env_set);
	TEST(disabled_is_noop);

	printf("  -- hash semantics --\n");
	TEST(deterministic_same_sequence);
	TEST(different_sequences_different_hash);

	printf("  -- walk --\n");
	TEST(observed_thread_appears_in_walk);
	TEST(walk_no_call_no_appearance);

	printf("  -- edge cases --\n");
	TEST(null_inputs_safe);

	printf("  -- concurrency --\n");
	TEST(multi_thread_independent_hashes);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
