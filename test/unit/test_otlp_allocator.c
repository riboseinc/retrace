/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the otlp-c allocator shim
 * (src/core/otlp_allocator.c -- the architecture deepening).
 *
 * The shim is subtle on purpose: an 8-byte size header lets
 * realloc bound its copy (the v2.35.0 musl heap-overflow
 * lesson). These tests pin the contract the OTLP waves and the
 * planned supervisor agent rely on:
 *
 *   - alloc/free round-trip; free(NULL) is safe
 *   - realloc grow: every old byte survives
 *   - realloc shrink: prefix survives
 *   - realloc(NULL, n) == alloc(n)
 *   - alignment: payload is at least void*-aligned
 *   - neighbor isolation: filling one block never touches the
 *     next (the header is invisible to the caller)
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "otlp_allocator.h"

static struct retrace_otlp_allocator_hooks H;
static int hooks_ready;

static void hooks(void)
{
	if (!hooks_ready) {
		retrace_otlp_allocator_install();
		retrace_otlp_allocator_hooks(&H);
		hooks_ready = 1;
	}
}

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

static void test_alloc_free_roundtrip(void)
{
	unsigned char *p;

	hooks();
	p = H.alloc(64);
	assert(p != NULL);
	memset(p, 0xA5, 64);
	H.free(p);
}

static void test_free_null_safe(void)
{
	hooks();
	H.free(NULL);
}

static void test_realloc_grow_preserves(void)
{
	unsigned char *p, *q;
	size_t i;

	hooks();
	p = H.alloc(16);
	assert(p != NULL);
	for (i = 0; i < 16; i++)
		p[i] = (unsigned char)(i * 7);
	q = H.realloc(p, 128);
	assert(q != NULL);
	for (i = 0; i < 16; i++)
		assert(q[i] == (unsigned char)(i * 7));
	H.free(q);
}

static void test_realloc_shrink_keeps_prefix(void)
{
	unsigned char *p, *q;
	size_t i;

	hooks();
	p = H.alloc(64);
	assert(p != NULL);
	for (i = 0; i < 64; i++)
		p[i] = (unsigned char)(i + 1);
	q = H.realloc(p, 8);
	assert(q != NULL);
	for (i = 0; i < 8; i++)
		assert(q[i] == (unsigned char)(i + 1));
	H.free(q);
}

static void test_realloc_null_is_alloc(void)
{
	unsigned char *p;

	hooks();
	p = H.realloc(NULL, 32);
	assert(p != NULL);
	memset(p, 0x5A, 32);
	H.free(p);
}

static void test_alignment(void)
{
	void *p;

	hooks();
	p = H.alloc(24);
	assert(p != NULL);
	assert(((uintptr_t)p % _Alignof(void *)) == 0);
	H.free(p);
}

static void test_neighbor_isolation(void)
{
	unsigned char *a, *b;
	size_t i;

	hooks();
	a = H.alloc(8);
	b = H.alloc(8);
	assert(a != NULL && b != NULL);
	memset(a, 0xFF, 8);
	for (i = 0; i < 8; i++)
		assert(b[i] == 0x00 || b[i] != 0xFF); /* untouched */
	H.free(a);
	H.free(b);
}

int main(void)
{
	printf("otlp_allocator tests:\n");

	printf("  -- shim semantics --\n");
	TEST(alloc_free_roundtrip);
	TEST(free_null_safe);
	TEST(realloc_grow_preserves);
	TEST(realloc_shrink_keeps_prefix);
	TEST(realloc_null_is_alloc);
	TEST(alignment);

	printf("  -- isolation --\n");
	TEST(neighbor_isolation);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
