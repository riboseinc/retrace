/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the reentrance_guard module (TODO.complete/13).
 *
 * The guard is a thin abstraction over the in-use marker on
 * ThreadContext. Tests verify:
 *
 *   - active() returns 0 on a freshly-zeroed context
 *   - active() returns 1 after enter()
 *   - enter() captures both real_impl and arch_spec_ctx
 *   - active() returns 0 after memset(0) (simulating
 *     thread_context_clear)
 *   - enter() can be called repeatedly (overwrites prior state)
 *
 * Part of TODO.complete/13 (engine MECE refactor).
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "engine.h"
#include "reentrance_guard.h"

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

static void test_fresh_context_inactive(void)
{
	struct ThreadContext ctx;

	memset(&ctx, 0, sizeof(ctx));
	assert(retrace_reentrance_guard_active(&ctx) == 0);
}

static void test_active_after_enter(void)
{
	struct ThreadContext ctx;
	int marker;

	memset(&ctx, 0, sizeof(ctx));
	retrace_reentrance_guard_enter(&ctx, &marker, NULL);
	assert(retrace_reentrance_guard_active(&ctx) == 1);
}

static void test_enter_captures_real_impl(void)
{
	struct ThreadContext ctx;
	int real_marker;
	int arch_marker;

	memset(&ctx, 0, sizeof(ctx));
	retrace_reentrance_guard_enter(&ctx, &real_marker, &arch_marker);
	assert(ctx.real_impl == &real_marker);
}

static void test_enter_captures_arch_spec_ctx(void)
{
	struct ThreadContext ctx;
	int real_marker;
	int arch_marker;

	memset(&ctx, 0, sizeof(ctx));
	retrace_reentrance_guard_enter(&ctx, &real_marker, &arch_marker);
	assert(ctx.arch_spec_ctx == &arch_marker);
}

static void test_inactive_after_clear(void)
{
	struct ThreadContext ctx;
	int marker;

	memset(&ctx, 0, sizeof(ctx));
	retrace_reentrance_guard_enter(&ctx, &marker, NULL);

	/* thread_context_clear does memset(0). */
	memset(&ctx, 0, sizeof(ctx));
	assert(retrace_reentrance_guard_active(&ctx) == 0);
	assert(ctx.real_impl == NULL);
	assert(ctx.arch_spec_ctx == NULL);
}

static void test_repeated_enter_overwrites(void)
{
	struct ThreadContext ctx;
	int first_marker;
	int second_marker;

	memset(&ctx, 0, sizeof(ctx));

	retrace_reentrance_guard_enter(&ctx, &first_marker, NULL);
	assert(ctx.real_impl == &first_marker);

	retrace_reentrance_guard_enter(&ctx, &second_marker, NULL);
	assert(ctx.real_impl == &second_marker);
	assert(retrace_reentrance_guard_active(&ctx) == 1);
}

static void test_null_real_impl_is_inactive(void)
{
	/* Edge case: if real_impl is NULL, active() returns 0 even
	 * after enter() was called with NULL. This matches the
	 * engine's invariant: real_impl is always non-NULL when
	 * enter() is called (engine.c checks for NULL and returns
	 * early before reaching enter).
	 */
	struct ThreadContext ctx;

	memset(&ctx, 0, sizeof(ctx));
	retrace_reentrance_guard_enter(&ctx, NULL, NULL);
	assert(retrace_reentrance_guard_active(&ctx) == 0);
}

/* TODO 31: the "permanent" variant -- held by the otlp-c
 * exporter thread for its lifetime so its send()/connect()
 * reach the real socket implementation, not recurse.
 */
static void test_permanent_holds_across_calls(void);

int main(void)
{
	printf("reentrance_guard tests:\n");

	printf("  -- active() semantics --\n");
	TEST(fresh_context_inactive);
	TEST(active_after_enter);
	TEST(inactive_after_clear);
	TEST(null_real_impl_is_inactive);
	TEST(permanent_holds_across_calls);

	printf("  -- enter() captures --\n");
	TEST(enter_captures_real_impl);
	TEST(enter_captures_arch_spec_ctx);

	printf("  -- repeated enter --\n");
	TEST(repeated_enter_overwrites);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}

/* TODO 31: the "permanent" variant -- held by the otlp-c
 * exporter thread for its lifetime so its send()/connect()
 * reach the real socket implementation, not recurse.
 */
static void test_permanent_holds_across_calls(void)
{
	struct ThreadContext ctx;
	void *arch = (void *)0xbeef;
	int i;

	memset(&ctx, 0, sizeof(ctx));
	retrace_reentrance_guard_enter_permanent(&ctx, arch);
	assert(retrace_reentrance_guard_active(&ctx) == 1);
	/* Subsequent wrapper entries on this thread see it active
	 * (the permanent marker survives any clear-like teardown).
	 */
	for (i = 0; i < 5; i++)
		assert(retrace_reentrance_guard_active(&ctx) == 1);
}
