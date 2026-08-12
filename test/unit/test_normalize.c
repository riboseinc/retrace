/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the trace-diff normalizer (TODO.complete/27).
 *
 * Covers:
 *   - normalize_from_trace aggregates call_count per function
 *   - normalize_from_trace sums call_duration_us per function
 *   - normalize_from_trace skips entries without message.func
 *     (engine noise)
 *   - normalize_from_trace skips entries without call_duration_us
 *   - normalize_from_trace NULL safety
 *   - normalize_find returns the right FuncStat, NULL for missing
 *   - normalize_call_sequence preserves call order
 *   - normalize_call_sequence filters engine noise
 *   - normalize_free is safe on initialized and zeroed logs
 *
 * Note: function calls live OUTSIDE assert() so the side-effecting
 * call still happens under -DNDEBUG.
 */

#include "parson.h"
#include "normalize.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

/* ----- Helpers ----- */

static JSON_Array *trace_from_string(const char *src)
{
	JSON_Value *v = json_parse_string(src);

	return json_value_get_array(v);
}

static void trace_free(JSON_Array *a)
{
	json_value_free(json_array_get_wrapping_value(a));
}

static void trace_append_call(JSON_Array *a, const char *func,
			      double duration_us)
{
	JSON_Value *entry = json_value_init_object();
	JSON_Object *entry_o = json_value_get_object(entry);
	JSON_Value *msg_v = json_value_init_object();
	JSON_Object *msg = json_value_get_object(msg_v);

	json_object_set_string(msg, "func", func);
	json_object_set_number(msg, "call_duration_us", duration_us);
	json_object_set_value(entry_o, "message", msg_v);
	json_array_append_value(a, entry);
}

static void trace_append_noise(JSON_Array *a, const char *text)
{
	JSON_Value *entry = json_value_init_object();
	JSON_Object *entry_o = json_value_get_object(entry);
	JSON_Value *msg_v = json_value_init_object();
	JSON_Object *msg = json_value_get_object(msg_v);

	json_object_set_string(msg, "text", text);
	json_object_set_value(entry_o, "message", msg_v);
	json_array_append_value(a, entry);
}

/* ----- normalize_from_trace ----- */

static void test_aggregates_call_count(void)
{
	JSON_Array *t = trace_from_string("[]");
	struct NormalizedLog log;
	const struct FuncStat *st;
	int rc;

	trace_append_call(t, "malloc", 10.0);
	trace_append_call(t, "malloc", 20.0);
	trace_append_call(t, "malloc", 30.0);
	trace_append_call(t, "free", 5.0);

	rc = normalize_from_trace(t, &log);
	CHECK(rc == 0);
	CHECK(log.count == 2);

	st = normalize_find(&log, "malloc");
	CHECK(st != NULL);
	CHECK(st->call_count == 3);

	st = normalize_find(&log, "free");
	CHECK(st != NULL);
	CHECK(st->call_count == 1);

	normalize_free(&log);
	trace_free(t);
}

static void test_sums_duration(void)
{
	JSON_Array *t = trace_from_string("[]");
	struct NormalizedLog log;
	const struct FuncStat *st;
	int rc;

	trace_append_call(t, "malloc", 10.0);
	trace_append_call(t, "malloc", 20.0);
	trace_append_call(t, "malloc", 30.0);

	rc = normalize_from_trace(t, &log);
	CHECK(rc == 0);
	st = normalize_find(&log, "malloc");
	CHECK(st != NULL);
	CHECK(st->total_duration_us == 60);
	normalize_free(&log);
	trace_free(t);
}

static void test_skips_engine_noise(void)
{
	JSON_Array *t = trace_from_string("[]");
	struct NormalizedLog log;
	const struct FuncStat *st;
	int rc;

	trace_append_noise(t, "engine starting");
	trace_append_call(t, "open", 1.0);
	trace_append_noise(t, "engine stopping");

	rc = normalize_from_trace(t, &log);
	CHECK(rc == 0);
	CHECK(log.count == 1);
	st = normalize_find(&log, "open");
	CHECK(st != NULL);
	CHECK(st->call_count == 1);
	normalize_free(&log);
	trace_free(t);
}

static void test_skips_entries_without_duration(void)
{
	JSON_Array *t = trace_from_string("[]");
	JSON_Value *entry = json_value_init_object();
	JSON_Object *entry_o = json_value_get_object(entry);
	JSON_Value *msg_v = json_value_init_object();
	JSON_Object *msg = json_value_get_object(msg_v);
	struct NormalizedLog log;
	int rc;

	json_object_set_string(msg, "func", "open");
	json_object_set_value(entry_o, "message", msg_v);
	json_array_append_value(t, entry);

	trace_append_call(t, "malloc", 5.0);

	rc = normalize_from_trace(t, &log);
	CHECK(rc == 0);
	CHECK(log.count == 1);
	CHECK(normalize_find(&log, "open") == NULL);
	CHECK(normalize_find(&log, "malloc") != NULL);
	normalize_free(&log);
	trace_free(t);
}

static void test_null_safety(void)
{
	struct NormalizedLog log;

	CHECK(normalize_from_trace(NULL, &log) == -1);
	CHECK(normalize_from_trace(NULL, NULL) == -1);
}

static void test_empty_trace_yields_empty_log(void)
{
	JSON_Array *t = trace_from_string("[]");
	struct NormalizedLog log;
	int rc;

	rc = normalize_from_trace(t, &log);
	CHECK(rc == 0);
	CHECK(log.count == 0);
	normalize_free(&log);
	trace_free(t);
}

/* ----- normalize_find ----- */

static void test_find_missing_returns_null(void)
{
	JSON_Array *t = trace_from_string("[]");
	struct NormalizedLog log;
	int rc;

	trace_append_call(t, "malloc", 1.0);
	rc = normalize_from_trace(t, &log);
	CHECK(rc == 0);
	CHECK(normalize_find(&log, "free") == NULL);
	CHECK(normalize_find(NULL, "malloc") == NULL);
	CHECK(normalize_find(&log, NULL) == NULL);
	normalize_free(&log);
	trace_free(t);
}

/* ----- normalize_call_sequence ----- */

static void test_sequence_preserves_order(void)
{
	JSON_Array *t = trace_from_string("[]");
	char **seq = NULL;
	size_t len = 0;
	int rc;
	size_t i;

	trace_append_call(t, "open", 1.0);
	trace_append_call(t, "read", 2.0);
	trace_append_call(t, "close", 3.0);
	trace_append_call(t, "open", 4.0);

	rc = normalize_call_sequence(t, &seq, &len);
	CHECK(rc == 0);
	CHECK(len == 4);
	CHECK(strcmp(seq[0], "open") == 0);
	CHECK(strcmp(seq[1], "read") == 0);
	CHECK(strcmp(seq[2], "close") == 0);
	CHECK(strcmp(seq[3], "open") == 0);

	for (i = 0; i < len; i++)
		free(seq[i]);
	free(seq);
	trace_free(t);
}

static void test_sequence_filters_noise(void)
{
	JSON_Array *t = trace_from_string("[]");
	char **seq = NULL;
	size_t len = 0;
	int rc;

	trace_append_noise(t, "noise A");
	trace_append_call(t, "open", 1.0);
	trace_append_noise(t, "noise B");
	trace_append_call(t, "close", 2.0);

	rc = normalize_call_sequence(t, &seq, &len);
	CHECK(rc == 0);
	CHECK(len == 2);
	CHECK(strcmp(seq[0], "open") == 0);
	CHECK(strcmp(seq[1], "close") == 0);

	free(seq[0]);
	free(seq[1]);
	free(seq);
	trace_free(t);
}

static void test_sequence_null_safety(void)
{
	char **seq = NULL;
	size_t len = 0;

	CHECK(normalize_call_sequence(NULL, &seq, &len) == -1);
}

/* ----- normalize_free ----- */

static void test_free_on_zeroed_log_is_safe(void)
{
	struct NormalizedLog log;

	memset(&log, 0, sizeof(log));
	normalize_free(&log);
	CHECK(log.funcs == NULL);
	CHECK(log.count == 0);
}

static void test_free_null_is_safe(void)
{
	normalize_free(NULL);
}

int main(void)
{
	printf("-- normalize_from_trace --\n");
	TEST(aggregates_call_count);
	TEST(sums_duration);
	TEST(skips_engine_noise);
	TEST(skips_entries_without_duration);
	TEST(null_safety);
	TEST(empty_trace_yields_empty_log);

	printf("-- normalize_find --\n");
	TEST(find_missing_returns_null);

	printf("-- normalize_call_sequence --\n");
	TEST(sequence_preserves_order);
	TEST(sequence_filters_noise);
	TEST(sequence_null_safety);

	printf("-- normalize_free --\n");
	TEST(free_on_zeroed_log_is_safe);
	TEST(free_null_is_safe);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
