/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for profile drift + validation
 * (TODO.trace-profile/02, 06).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "aggregate.h"
#include "diff.h"
#include "match.h"
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

static void add_json(struct Profile *p, const char *entry)
{
	JSON_Value *v = json_parse_string(entry);

	CHECK(v != NULL);
	prof_add_entry(json_value_get_object(v), p);
	json_value_free(v);
}

static void feed(struct Profile *p, const char *const *entries)
{
	size_t i;

	prof_init(p);
	for (i = 0; entries[i] != NULL; i++)
		add_json(p, entries[i]);
	prof_finish(p);
}

/* -- drift -- */

static void test_no_drift(void)
{
	static const char *const e[] = {
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/x.dat\"}}}",
		NULL
	};
	struct Profile a, b;
	struct ProfDiff d;
	int drift;

	feed(&a, e);
	feed(&b, e);
	prof_diff_init(&d);
	drift = prof_diff_compute(&a, &b, &d);
	CHECK(drift == 0);
	CHECK(d.count == 0);
	CHECK(d.new_functions_cnt == 0);
	prof_diff_free(&d);
	prof_free(&a);
	prof_free(&b);
}

static void test_new_path(void)
{
	static const char *const base[] = {
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/x.dat\"}}}",
		NULL
	};
	static const char *const cand[] = {
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/x.dat\"}}}",
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/new.dat\"}}}",
		NULL
	};
	struct Profile a, b;
	struct ProfDiff d;

	feed(&a, base);
	feed(&b, cand);
	prof_diff_init(&d);
	CHECK(prof_diff_compute(&a, &b, &d) == 1);
	CHECK(d.count == 1);
	CHECK(strcmp(d.changes[0].path, "/a/new.dat") == 0);
	CHECK(d.changes[0].class_from == -1);
	CHECK(d.changes[0].class_to == CORR_CLS_READ);
	prof_diff_free(&d);
	prof_free(&a);
	prof_free(&b);
}

static void test_removed_path(void)
{
	static const char *const base[] = {
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/old.tmp\"}}}",
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/x.dat\"}}}",
		NULL
	};
	static const char *const cand[] = {
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/x.dat\"}}}",
		NULL
	};
	struct Profile a, b;
	struct ProfDiff d;

	feed(&a, base);
	feed(&b, cand);
	prof_diff_init(&d);
	CHECK(prof_diff_compute(&a, &b, &d) == 1);
	CHECK(d.count == 1);
	CHECK(strcmp(d.changes[0].path, "/a/old.tmp") == 0);
	CHECK(d.changes[0].class_to == -1);
	prof_diff_free(&d);
	prof_free(&a);
	prof_free(&b);
}

static void test_class_escalation(void)
{
	/* read in baseline; write (creat) in candidate */
	static const char *const base[] = {
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/cfg.dat\"}}}",
		NULL
	};
	static const char *const cand[] = {
		"{\"message\":{\"func\":\"creat\",\"params\":{\"path\":\"/a/cfg.dat\"}}}",
		NULL
	};
	struct Profile a, b;
	struct ProfDiff d;

	feed(&a, base);
	feed(&b, cand);
	prof_diff_init(&d);
	CHECK(prof_diff_compute(&a, &b, &d) == 1);
	CHECK(d.count == 1);
	CHECK(strcmp(d.changes[0].path, "/a/cfg.dat") == 0);
	CHECK(d.changes[0].class_from == CORR_CLS_READ);
	CHECK(d.changes[0].class_to == CORR_CLS_WRITE);
	prof_diff_free(&d);
	prof_free(&a);
	prof_free(&b);
}

static void test_new_function(void)
{
	static const char *const base[] = {
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/x.dat\"}}}",
		NULL
	};
	static const char *const cand[] = {
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/x.dat\"}}}",
		"{\"message\":{\"func\":\"fopen64\",\"params\":{\"path\":\"/a/x.dat\"}}}",
		NULL
	};
	struct Profile a, b;
	struct ProfDiff d;

	feed(&a, base);
	feed(&b, cand);
	prof_diff_init(&d);
	CHECK(prof_diff_compute(&a, &b, &d) == 1);
	CHECK(d.new_functions_cnt == 1);
	CHECK(strcmp(d.new_functions[0], "fopen64") == 0);
	prof_diff_free(&d);
	prof_free(&a);
	prof_free(&b);
}

static void test_diff_json_shape(void)
{
	static const char *const base[] = {
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/x.dat\"}}}",
		NULL
	};
	static const char *const cand[] = {
		"{\"message\":{\"func\":\"creat\",\"params\":{\"path\":\"/a/new.dat\"}}}",
		NULL
	};
	struct Profile a, b;
	struct ProfDiff d;
	JSON_Value *v;

	feed(&a, base);
	feed(&b, cand);
	prof_diff_init(&d);
	(void)prof_diff_compute(&a, &b, &d);
	v = prof_diff_to_json(&d);
	CHECK(v != NULL);
	CHECK(json_object_get_boolean(json_value_get_object(v),
		"drift") == 1);
	CHECK(json_array_get_count(json_object_get_array(
		json_value_get_object(v), "path_changes")) == 2);
	json_value_free(v);
	prof_diff_free(&d);
	prof_free(&a);
	prof_free(&b);
}

/* -- validation -- */

#define PROFILE_TEST_FILE "prof_test_profile.json"

static void write_profile(const char *text)
{
	FILE *f = fopen(PROFILE_TEST_FILE, "wb");

	CHECK(f != NULL);
	fwrite(text, 1, strlen(text), f);
	fclose(f);
}

extern int prof_validate_file(const char *path, char *err,
			      size_t errsz);

static void test_validate_ok(void)
{
	char err[256];

	write_profile(
		"{\"profile\":{\"entries\":1,\"functions\":[],"
		"\"env\":[],\"net\":[],\"accesses\":"
		"[{\"path\":\"/a\",\"class\":\"read\",\"hits\":1}]},"
		"\"coverage\":{\"libc_layer\":\"captured\","
		"\"kernel_layer\":\"ABSENT\"}}");
	CHECK(prof_validate_file(PROFILE_TEST_FILE, err,
		sizeof(err)) == 0);
	remove(PROFILE_TEST_FILE);
}

static void test_validate_bad_class(void)
{
	char err[256];
	int n;

	write_profile(
		"{\"profile\":{\"entries\":1,\"functions\":[],"
		"\"env\":[],\"net\":[],\"accesses\":"
		"[{\"path\":\"/a\",\"class\":\"rw\",\"hits\":1}]},"
		"\"coverage\":{\"libc_layer\":\"captured\","
		"\"kernel_layer\":\"ABSENT\"}}");
	n = prof_validate_file(PROFILE_TEST_FILE, err, sizeof(err));
	CHECK(n == 1);
	CHECK(strstr(err, "class") != NULL);
	remove(PROFILE_TEST_FILE);
}

static void test_validate_risk_cross_field(void)
{
	char err[256];
	int n;

	/* kernel captured but no risk section */
	write_profile(
		"{\"profile\":{\"entries\":0,\"functions\":[],"
		"\"env\":[],\"net\":[],\"accesses\":[]},"
		"\"coverage\":{\"libc_layer\":\"captured\","
		"\"kernel_layer\":\"captured\"}}");
	n = prof_validate_file(PROFILE_TEST_FILE, err, sizeof(err));
	CHECK(n == 1);
	CHECK(strstr(err, "risk") != NULL);
	remove(PROFILE_TEST_FILE);
}

static void test_validate_unparseable(void)
{
	char err[256];

	write_profile("not json at all");
	CHECK(prof_validate_file(PROFILE_TEST_FILE, err,
		sizeof(err)) < 0);
	remove(PROFILE_TEST_FILE);
}

int main(void)
{
	printf("profile drift tests:\n");
	TEST(no_drift);
	TEST(new_path);
	TEST(removed_path);
	TEST(class_escalation);
	TEST(new_function);
	TEST(diff_json_shape);

	printf("profile validation tests:\n");
	TEST(validate_ok);
	TEST(validate_bad_class);
	TEST(validate_risk_cross_field);
	TEST(validate_unparseable);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
