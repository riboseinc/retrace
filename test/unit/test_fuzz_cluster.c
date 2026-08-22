/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for fuzz-report clustering (TODO.trace-profile/20):
 * signature hashing, clean/crash/assertion classification,
 * cluster merging, truncated-trace tolerance, JSON shape.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cluster.h"
#include "parson.h"

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

/* Always-on check: assert() compiles to nothing under NDEBUG. */
#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

static void test_clean_runs_not_clustered(void)
{
	struct FuzzReport r;

	fuzz_report_init(&r);
	CHECK(fuzz_report_fold(&r, 0, "[]", 1, NULL) == 0);
	CHECK(fuzz_report_fold(&r, 0, NULL, 2, NULL) == 0);
	CHECK(r.total == 2);
	CHECK(r.crashes == 0);
	CHECK(r.count == 0);
	fuzz_report_free(&r);
}

static void test_crashes_cluster_by_func(void)
{
	struct FuzzReport r;
	unsigned long a, b;

	fuzz_report_init(&r);
	/* 139 = shell-ish signal death stand-in: use a real status
	 * shape -- WIFSIGNALED only decodes waitpid statuses; on
	 * POSIX build a SIGSEGV status via the macro-free form
	 */
	a = fuzz_report_fold(&r, 0x000b, /* SIGSEGV low byte */
		"[{\"message\":{\"func\":\"malloc\",\"params\":{"
		"\"size\":\"64\"}}}]",
		10, NULL);
	b = fuzz_report_fold(&r, 0x000b,
		"[{\"message\":{\"func\":\"malloc\",\"params\":{"
		"\"size\":\"32\"}}}]",
		11, NULL);
	CHECK(r.crashes == 2);
	CHECK(a == b);            /* same func -> same cluster */
	CHECK(r.count == 1);
	CHECK(r.clusters[0].count == 2);
	fuzz_report_free(&r);
}

static void test_different_funcs_dont_merge(void)
{
	struct FuzzReport r;
	unsigned long a, b;

	fuzz_report_init(&r);
	a = fuzz_report_fold(&r, 0x000b,
		"[{\"message\":{\"func\":\"malloc\",\"params\":{}}}]",
		1, NULL);
	b = fuzz_report_fold(&r, 0x000b,
		"[{\"message\":{\"func\":\"strcpy\",\"params\":{}}}]",
		2, NULL);
	CHECK(a != b);
	CHECK(r.count == 2);
	fuzz_report_free(&r);
}

static void test_truncated_trace_tolerated(void)
{
	struct FuzzReport r;
	unsigned long id;

	fuzz_report_init(&r);
	/* crash mid-write: one complete entry, then a truncated
	 * tail (no closing braces) -- the tolerant scanner
	 * delivers the complete entry, skips the tail
	 */
	id = fuzz_report_fold(&r, 0x000b,
		"[{\"message\":{\"func\":\"fopen\",\"params\":{"
		"\"path\":\"/a\"}}},{\"message\":{\"func\":\"mall",
		1, NULL);
	CHECK(r.count == 1);
	CHECK(strcmp(r.clusters[0].func, "fopen") == 0);
	CHECK(id != 0);
	fuzz_report_free(&r);
}

static void test_empty_trace_unattributable(void)
{
	struct FuzzReport r;

	fuzz_report_init(&r);
	fuzz_report_fold(&r, 0x000b, "", 1, NULL);
	CHECK(r.count == 1);
	CHECK(strcmp(r.clusters[0].func, "?") == 0);
	fuzz_report_free(&r);
}

static void test_assertion_marker(void)
{
	struct FuzzReport r;
	unsigned long id;

	fuzz_report_init(&r);
	/* exit 0 but the marker in the trace = assertion */
	id = fuzz_report_fold(&r, 0,
		"[{\"message\":{\"text\":\"ASSERT: bad state\"}}]",
		5, "ASSERT");
	CHECK(id != 0);
	CHECK(r.assertions == 1);
	CHECK(r.crashes == 0);
	CHECK(r.clusters[0].is_crash == 0); /* assertion, not signal */
	fuzz_report_free(&r);
}

static void test_json_shape(void)
{
	struct FuzzReport r;
	JSON_Value *v;
	JSON_Object *o;
	JSON_Array *arr;
	JSON_Object *c;
	char *ser;

	fuzz_report_init(&r);
	fuzz_report_fold(&r, 0x000b,
		"[{\"message\":{\"func\":\"malloc\",\"params\":{}}}]",
		123456789012345UL, NULL);
	v = fuzz_report_to_json(&r);
	o = json_value_get_object(v);
	CHECK(json_object_get_number(o, "iterations") == 1);
	CHECK(json_object_get_number(o, "crashes") == 1);
	arr = json_object_get_array(o, "clusters");
	CHECK(json_array_get_count(arr) == 1);
	c = json_array_get_object(arr, 0);
	/* id and seed are STRINGS: > 2^53 values survive JSON */
	CHECK(json_object_get_string(c, "id") != NULL);
	CHECK(json_object_get_string(c, "seed") != NULL);
	CHECK(strcmp(json_object_get_string(c, "seed"),
		"123456789012345") == 0);
	CHECK(strcmp(json_object_get_string(c, "kind"), "crash")
		== 0);

	ser = json_serialize_to_string(v);
	CHECK(strstr(ser, "\"iterations\"") != NULL);
	json_free_serialized_string(ser);
	json_value_free(v);
	fuzz_report_free(&r);
}

int main(void)
{
	printf("fuzz cluster tests:\n");
	TEST(clean_runs_not_clustered);
	TEST(crashes_cluster_by_func);
	TEST(different_funcs_dont_merge);
	TEST(truncated_trace_tolerated);
	TEST(empty_trace_unattributable);
	TEST(assertion_marker);
	TEST(json_shape);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
