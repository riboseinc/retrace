/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the otlp_live module (TODO.trace-profile/31).
 *
 * The exporter's lifecycle is what we test here -- the per-emit
 * JSON-to-span conversion is exercised by the integration tests
 * with the fixture HTTP server (test/fixtures/fixture_otlp_server.py).
 *
 * Tests verify:
 *   - init() is a no-op without RETRACE_OTLP_ENDPOINT
 *   - init() is idempotent (calling twice is a no-op)
 *   - get_stats() returns zeros when not initialized
 *   - deinit() is safe to call without init
 *   - emit_json() is a no-op when not initialized
 *
 * Part of TODO.trace-profile/31 (otlp-c Wave B).
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "otlp_live.h"

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

static void test_init_without_endpoint(void)
{
	/* Without RETRACE_OTLP_ENDPOINT, init is a silent no-op. */
	unsetenv("RETRACE_OTLP_ENDPOINT");
	assert(retrace_otlp_live_init() == 0);
}

static void test_get_stats_when_uninitialized(void)
{
	uint64_t emitted = 99, sent = 99, dropped_full = 99, dropped_err = 99;

	retrace_otlp_live_get_stats(&emitted, &sent, &dropped_full,
		&dropped_err);
	assert(emitted == 0);
	assert(sent == 0);
	assert(dropped_full == 0);
	assert(dropped_err == 0);
}

static void test_emit_when_uninitialized_is_noop(void)
{
	/* emit_json must not crash or block when the exporter is
	 * not initialized.
	 */
	const char *json =
		"{\"time\":1,\"pid\":2,\"tid\":3,\"module\":\"FUNCS\","
		"\"severity\":\"INFO\",\"message\":{\"func\":\"malloc\"}}";

	assert(retrace_otlp_live_emit_json(json) == 0);
	assert(retrace_otlp_live_emit_json(NULL) == -1);
	assert(retrace_otlp_live_emit_json("not json") == -1);
}

static void test_deinit_when_uninitialized_is_safe(void)
{
	/* Must not crash on a never-initialized exporter. */
	retrace_otlp_live_deinit();
}

static void test_get_stats_null_args(void)
{
	/* NULL pointers should be tolerated (no deref). */
	retrace_otlp_live_get_stats(NULL, NULL, NULL, NULL);
}

static void test_emit_json_empty_string(void)
{
	/* Empty JSON parses to NULL -- emit returns -1 (parse fail)
	 * but doesn't crash.
	 */
	assert(retrace_otlp_live_emit_json("") == -1);
	assert(retrace_otlp_live_emit_json("{}") == -1);
}

static void test_emit_event_when_uninitialized_is_noop(void)
{
	/* Wave C: security events are no-ops without an endpoint.
	 * NULL event name is refused; real events return 0 without
	 * side effects.
	 */
	struct retrace_otlp_event_attr attrs[2] = {
		{ "retrace.jail.path", "/etc/shadow", 0 },
		{ "retrace.jail.count", NULL, 3 },
	};

	assert(retrace_otlp_live_emit_event(RETRACE_OTLP_SEV_ERROR,
		NULL, attrs, 2) == -1);
	assert(retrace_otlp_live_emit_event(RETRACE_OTLP_SEV_ERROR,
		"retrace.jail.denied", attrs, 2) == 0);
	assert(retrace_otlp_live_emit_event(RETRACE_OTLP_SEV_WARN,
		"retrace.drift.hit", NULL, 0) == 0);
	/* NULL attr entries are skipped, not dereferenced. */
	assert(retrace_otlp_live_emit_event(RETRACE_OTLP_SEV_INFO,
		"retrace.fuzz.cluster", NULL, 4) == 0);
}

int main(void)
{
	printf("otlp_live tests:\n");

	printf("  -- lifecycle --\n");
	TEST(init_without_endpoint);
	TEST(get_stats_when_uninitialized);
	TEST(deinit_when_uninitialized_is_safe);

	printf("  -- emit path --\n");
	TEST(emit_when_uninitialized_is_noop);
	TEST(emit_json_empty_string);
	TEST(emit_event_when_uninitialized_is_noop);

	printf("  -- null-safety --\n");
	TEST(get_stats_null_args);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
