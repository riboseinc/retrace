/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the caller_cache module (TODO.complete/17 P1).
 *
 * Verifies the cache stores (ret_addr -> Dl_info) mappings and
 * returns hits without calling dladdr again.
 *
 * Tests:
 *   - empty cache returns miss
 *   - insert + lookup = hit
 *   - lookup with different ret_addr = miss
 *   - insert twice with same ret_addr updates the entry
 *   - stats counters increment correctly
 *   - clear() empties the cache and resets stats
 *   - NULL inputs handled gracefully
 *
 * Part of TODO.complete/17.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "caller_cache.h"
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

static void test_empty_cache_misses(void)
{
	Dl_info info;
	int marker;

	retrace_caller_cache_clear();
	assert(retrace_caller_cache_lookup(&marker, &info) == 0);
}

static void test_insert_then_hit(void)
{
	Dl_info in = {0};
	Dl_info out;
	int marker;
	char fname_buf[] = "/test/module.so";
	char sname_buf[] = "test_symbol";

	in.dli_fname = fname_buf;
	in.dli_fbase = (void *)0x1000;
	in.dli_sname = sname_buf;
	in.dli_saddr = (void *)0x2000;

	retrace_caller_cache_clear();
	retrace_caller_cache_insert(&marker, &in);

	assert(retrace_caller_cache_lookup(&marker, &out) == 1);
	assert(out.dli_fbase == (void *)0x1000);
	assert(out.dli_saddr == (void *)0x2000);
}

static void test_different_addr_misses(void)
{
	Dl_info in = {0};
	Dl_info out;
	int marker1;
	int marker2;

	retrace_caller_cache_clear();
	retrace_caller_cache_insert(&marker1, &in);

	assert(retrace_caller_cache_lookup(&marker2, &out) == 0);
}

static void test_double_insert_updates(void)
{
	Dl_info in1 = {0};
	Dl_info in2 = {0};
	Dl_info out;
	int marker;

	in1.dli_sname = "first";
	in2.dli_sname = "second";

	retrace_caller_cache_clear();
	retrace_caller_cache_insert(&marker, &in1);
	retrace_caller_cache_insert(&marker, &in2);

	assert(retrace_caller_cache_lookup(&marker, &out) == 1);
	/* dli_sname points to the inserted string, so the latest
	 * value should be visible. The cache stores the Dl_info
	 * struct by value, including pointer fields.
	 */
	assert(out.dli_sname == in2.dli_sname);
}

static void test_stats_increment(void)
{
	Dl_info in = {0};
	Dl_info out;
	int marker;
	unsigned long hits, misses;

	retrace_caller_cache_clear();
	retrace_caller_cache_lookup(&marker, &out);  /* miss */
	retrace_caller_cache_lookup(&marker, &out);  /* miss */
	retrace_caller_cache_insert(&marker, &in);
	retrace_caller_cache_lookup(&marker, &out);  /* hit */
	retrace_caller_cache_lookup(&marker, &out);  /* hit */

	retrace_caller_cache_stats(&hits, &misses);
	assert(hits == 2);
	assert(misses == 2);
}

static void test_clear_empties(void)
{
	Dl_info in = {0};
	Dl_info out;
	int marker;

	retrace_caller_cache_insert(&marker, &in);
	retrace_caller_cache_clear();

	assert(retrace_caller_cache_lookup(&marker, &out) == 0);
}

static void test_null_inputs(void)
{
	Dl_info info;

	assert(retrace_caller_cache_lookup(NULL, &info) == 0);
	retrace_caller_cache_insert(NULL, &info);  /* no-op, no crash */
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

	/* Mutex ops -- caller_cache uses a static PTHREAD_MUTEX_INITIALIZER
	 * which doesn't need init, but lock/unlock go through real_impls.
	 */
	retrace_real_impls.pthread_mutex_lock = pthread_mutex_lock;
	retrace_real_impls.pthread_mutex_unlock = pthread_mutex_unlock;

	printf("caller_cache tests:\n");

	printf("  -- lookup/insert semantics --\n");
	TEST(empty_cache_misses);
	TEST(insert_then_hit);
	TEST(different_addr_misses);
	TEST(double_insert_updates);
	TEST(null_inputs);

	printf("  -- stats and clear --\n");
	TEST(stats_increment);
	TEST(clear_empties);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
