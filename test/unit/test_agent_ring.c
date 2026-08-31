/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * The agent's bounded event ring -- the module both platform
 * queues became. The contract that was drifting between the
 * two copies is pinned here: inline copies, oversize owns its
 * heap, every refusal is counted, order is kept, and the
 * peek/pop split holds the slot through the consumer's send.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent_ring.h"

static int tests_run;
static int tests_pass;
static int tests_fail;

#define TEST(name) do { \
	int before = tests_fail; \
	tests_run++; \
	printf("  TEST %s ... ", #name); \
	name(); \
	if (tests_fail == before) \
		tests_pass++; \
	printf("%s\n", tests_fail == before ? "ok" : ""); \
} while (0)

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("\n    FAIL %s:%d: %s\n", __func__, \
			__LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

static void test_order_and_peek_pop_split(void);
static void test_full_refusal_is_counted(void);
static void test_heap_refusal_frees_and_counts(void);
static void test_oversize_rides_the_heap(void);

static void test_order_and_peek_pop_split(void)
{
	struct agent_ring r;
	const char *v;
	size_t len;

	agent_ring_init(&r);
	CHECK(agent_ring_push_copy(&r, "one", 3) == 0);
	CHECK(agent_ring_push_copy(&r, "two", 3) == 0);
	v = agent_ring_peek(&r, &len);
	CHECK(v != NULL && strcmp(v, "one") == 0 && len == 3);
	/* the peek holds: a push cannot overwrite the view */
	CHECK(agent_ring_push_copy(&r, "three", 5) == 0);
	v = agent_ring_peek(&r, NULL);
	CHECK(v != NULL && strcmp(v, "one") == 0);
	agent_ring_pop(&r);
	v = agent_ring_peek(&r, NULL);
	CHECK(v != NULL && strcmp(v, "two") == 0);
	agent_ring_pop(&r);
	v = agent_ring_peek(&r, NULL);
	CHECK(v != NULL && strcmp(v, "three") == 0);
	agent_ring_pop(&r);
	CHECK(agent_ring_peek(&r, NULL) == NULL);
	agent_ring_pop(&r);	/* no-op on empty */
	CHECK(r.dropped == 0);
}

static void test_full_refusal_is_counted(void)
{
	struct agent_ring r;
	size_t i;

	agent_ring_init(&r);
	for (i = 0; i < AGENT_RING_CAP; i++)
		CHECK(agent_ring_push_copy(&r, "x", 1) == 0);
	CHECK(agent_ring_push_copy(&r, "y", 1) == -1);
	CHECK(r.dropped == 1);
	CHECK(agent_ring_push_copy(&r, "y", 1) == -1);
	CHECK(r.dropped == 2);
}

static void test_heap_refusal_frees_and_counts(void)
{
	struct agent_ring r;
	char *big;

	agent_ring_init(&r);
	big = (char *)malloc(AGENT_RING_INLINE + 64);
	CHECK(big != NULL);
	memset(big, 'h', AGENT_RING_INLINE + 63);
	big[AGENT_RING_INLINE + 63] = '\0';
	/* fill, then hand the ring an oversize item it must
	 * refuse: the heap is freed (the ring never leaks a
	 * refused item) and the drop is counted
	 */
	{
		size_t i;

		for (i = 0; i < AGENT_RING_CAP; i++)
			CHECK(agent_ring_push_copy(&r, "x", 1) == 0);
	}
	CHECK(agent_ring_push_heap(&r, big, strlen(big)) == -1);
	CHECK(r.dropped == 1);
}

static void test_oversize_rides_the_heap(void)
{
	struct agent_ring r;
	static char big[AGENT_RING_INLINE + 32];
	const char *v;
	size_t len;

	agent_ring_init(&r);
	memset(big, 'H', sizeof(big) - 1);
	big[sizeof(big) - 1] = '\0';
	CHECK(agent_ring_push_copy(&r, big, strlen(big)) == 0);
	CHECK(r.dropped == 0);
	v = agent_ring_peek(&r, &len);
	CHECK(v != NULL && len == strlen(big));
	CHECK(strlen(v) == strlen(big));
	CHECK(strncmp(v, big, len) == 0);
	agent_ring_pop(&r);
}

int main(void)
{
	printf("agent ring tests:\n");
	TEST(test_order_and_peek_pop_split);
	TEST(test_full_refusal_is_counted);
	TEST(test_heap_refusal_frees_and_counts);
	TEST(test_oversize_rides_the_heap);

	printf("%d tests: %d pass, %d fail\n", tests_run, tests_pass,
		tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
