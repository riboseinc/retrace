/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the shared tolerant trace loader
 * (TODO.windows/07): array document, JSONL, and truncated-tail
 * inputs must all yield the same parsed entries, so the offline
 * tools never see the log format.
 */

#include "parson.h"
#include "stream.h"
#include "trace_load.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run;
static int tests_pass;
static int tests_fail;

#define TEST(name)                               \
	do {                                     \
		tests_run++;                     \
		printf("  TEST %s ... ", #name); \
		test_##name();                   \
		tests_pass++;                    \
		printf("OK\n");                  \
	} while (0)

#define CHECK(cond)                                                             \
	do {                                                                    \
		if (!(cond)) {                                                  \
			printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
			tests_fail++;                                           \
			return;                                                 \
		}                                                               \
	} while (0)

static const char *g_entry_a =
	"{ \"time\": 1, \"pid\": 7, \"tid\": 8, \"module\": \"FUNCS\","
	" \"severity\": \"INFO\", \"message\": { \"func\": \"open\" } }";
static const char *g_entry_b =
	"{ \"time\": 2, \"pid\": 7, \"tid\": 9, \"module\": \"FUNCS\","
	" \"severity\": \"INFO\", \"message\": { \"func\": \"close\" } }";

static void
write_file(const char *path, const char *content)
{
	FILE *f = fopen(path, "wb");

	if (f == NULL)
		return;
	fwrite(content, 1, strlen(content), f);
	fclose(f);
}

static void
check_two_entries(const JSON_Value *arr)
{
	const JSON_Array *a = json_value_get_array((JSON_Value *)arr);
	JSON_Object *e0;
	JSON_Object *e1;

	CHECK(json_array_get_count(a) == 2);
	e0 = json_array_get_object(a, 0);
	e1 = json_array_get_object(a, 1);
	CHECK(e0 != NULL && e1 != NULL);
	CHECK(strcmp(json_object_get_string(json_object_get_object(
		e0, "message"), "func"), "open") == 0);
	CHECK(strcmp(json_object_get_string(json_object_get_object(
		e1, "message"), "func"), "close") == 0);
}

static void
test_array_document(void)
{
	char doc[1024];
	JSON_Value *arr;

	snprintf(doc, sizeof(doc), "[\n%s,\n%s\n]\n", g_entry_a,
		g_entry_b);
	write_file("/tmp/retrace-test-tl.json", doc);
	arr = trace_load_file("/tmp/retrace-test-tl.json", NULL);
	CHECK(arr != NULL);
	check_two_entries(arr);
	json_value_free(arr);
}

static void
test_jsonl(void)
{
	char doc[1024];
	JSON_Value *arr;

	snprintf(doc, sizeof(doc), "%s\n%s\n", g_entry_a, g_entry_b);
	write_file("/tmp/retrace-test-tl.json", doc);
	arr = trace_load_file("/tmp/retrace-test-tl.json", NULL);
	CHECK(arr != NULL);
	check_two_entries(arr);
	json_value_free(arr);
}

static void
test_truncated_tail(void)
{
	char doc[1024];
	JSON_Value *arr;

	/* Crashed trace: the last object never closes. */
	snprintf(doc, sizeof(doc), "[\n%s,\n%s,\n{ \"time\": 3, \"pi",
		g_entry_a, g_entry_b);
	write_file("/tmp/retrace-test-tl.json", doc);
	arr = trace_load_file("/tmp/retrace-test-tl.json", NULL);
	CHECK(arr != NULL);
	check_two_entries(arr);
	json_value_free(arr);
}

static void
test_missing_file(void)
{
	CHECK(trace_load_file("/tmp/retrace-test-tl-nope.json", NULL) ==
	      NULL);
}

static void
test_empty_file(void)
{
	JSON_Value *arr;

	write_file("/tmp/retrace-test-tl.json", "");
	arr = trace_load_file("/tmp/retrace-test-tl.json", NULL);
	CHECK(arr != NULL);
	CHECK(json_array_get_count(json_value_get_array(arr)) == 0);
	json_value_free(arr);
}

int
main(void)
{
	printf("trace load tests\n");
	TEST(array_document);
	TEST(jsonl);
	TEST(truncated_tail);
	TEST(missing_file);
	TEST(empty_file);
	printf("%d run, %d pass, %d fail\n", tests_run, tests_pass,
		tests_fail);
	return (tests_fail == 0) ? 0 : 1;
}
