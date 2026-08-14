/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Regression tests for parson's allocation-budget mechanism
 * (TODO.complete/33 P0; shipped in v2.3.0).
 *
 * Background: the nightly fuzz workflow found that a 129-byte
 * adversarial input could trigger ~2GB of allocation via the
 * comment-stripping path. The fix introduced an allocation budget
 * in json_parse_string_with_comments: input_len * 1000 bytes.
 *
 * These tests verify:
 *   - The budget does not reject realistic retrace configs.
 *   - The budget is reset between calls (no accumulation bug).
 *   - Edge cases (NULL, empty, whitespace-only) don't crash.
 *   - A config with comments (the path that originally OOM'd)
 *     still parses correctly.
 *
 * The tests cannot easily reproduce the exact 2GB amplification
 * without allocating gigabytes. Instead they guard against
 * regressions where someone sets the budget too tight (breaking
 * real configs) or fails to reset it between calls (breaking
 * repeated parses).
 */

#include "parson.h"

#include <stdarg.h>
#include <stdio.h>
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

/* A realistic retrace config: nested arrays + objects + multiple
 * intercept scripts. Exercises every JSON shape retrace uses.
 */
static const char *realistic_config =
	"{\n"
	"  \"intercept_scripts\": [\n"
	"    {\n"
	"      \"func_name\": \"*\",\n"
	"      \"actions\": [\n"
	"        { \"action_name\": \"log_params\" },\n"
	"        { \"action_name\": \"call_real\" }\n"
	"      ]\n"
	"    },\n"
	"    {\n"
	"      \"func_name\": \"malloc\",\n"
	"      \"actions\": [\n"
	"        { \"action_name\": \"memory_fuzz\",\n"
	"          \"action_params\": { \"fail_rate\": 0.1 } }\n"
	"      ]\n"
	"    }\n"
	"  ]\n"
	"}\n";

static const char *config_with_comments =
	"/* retrace default config */\n"
	"// log every call\n"
	"{\n"
	"  \"intercept_scripts\": [\n"
	"    /* wildcard matches every function */\n"
	"    {\n"
	"      \"func_name\": \"*\",\n"
	"      \"actions\": [\n"
	"        { \"action_name\": \"log_params\" }, // log args + retval\n"
	"        { \"action_name\": \"call_real\" }   // then call real impl\n"
	"      ]\n"
	"    }\n"
	"  ]\n"
	"}\n";

/* ----- Realistic configs still parse ----- */

static void test_realistic_config_parses(void)
{
	JSON_Value *v = json_parse_string_with_comments(realistic_config);

	CHECK(v != NULL);
	CHECK(json_value_get_type(v) == JSONObject);
	json_value_free(v);
}

static void test_config_with_comments_parses(void)
{
	JSON_Value *v = json_parse_string_with_comments(config_with_comments);

	CHECK(v != NULL);
	CHECK(json_value_get_type(v) == JSONObject);
	json_value_free(v);
}

static void test_minimal_object_parses(void)
{
	JSON_Value *v = json_parse_string_with_comments("{}");

	CHECK(v != NULL);
	CHECK(json_value_get_type(v) == JSONObject);
	json_value_free(v);
}

static void test_minimal_array_parses(void)
{
	JSON_Value *v = json_parse_string_with_comments("[]");

	CHECK(v != NULL);
	CHECK(json_value_get_type(v) == JSONArray);
	json_value_free(v);
}

/* ----- Budget resets between calls (no accumulation bug) ----- */

static void test_repeated_parse_does_not_accumulate(void)
{
	JSON_Value *v1, *v2, *v3;
	int i;

	/* 50 iterations would amplify any per-call leak by 50x; if
	 * the budget weren't reset, later calls would start rejecting.
	 */
	for (i = 0; i < 50; i++) {
		v1 = json_parse_string_with_comments(realistic_config);
		CHECK(v1 != NULL);
		json_value_free(v1);
	}

	v2 = json_parse_string_with_comments(config_with_comments);
	CHECK(v2 != NULL);
	json_value_free(v2);

	v3 = json_parse_string_with_comments(realistic_config);
	CHECK(v3 != NULL);
	json_value_free(v3);
}

/* ----- Edge cases don't crash ----- */

static void test_null_input_returns_null(void)
{
	JSON_Value *v = json_parse_string_with_comments(NULL);

	CHECK(v == NULL);
}

static void test_empty_string_returns_null(void)
{
	JSON_Value *v = json_parse_string_with_comments("");

	CHECK(v == NULL);
}

static void test_whitespace_only_returns_null(void)
{
	JSON_Value *v = json_parse_string_with_comments("\t\n\t");

	CHECK(v == NULL);
}

static void test_malformed_returns_null(void)
{
	JSON_Value *v = json_parse_string_with_comments("{not valid json");

	CHECK(v == NULL);
}

/* ----- Budget is generous enough for legitimate configs ----- */

/* Append helper that clamps `pos` before every snprintf, so
 * `sizeof(buf) - pos` can never underflow. CodeQL flags the
 * unguarded accumulation pattern as a size-argument overflow.
 */
static size_t append_chk(char *buf, size_t bufsz, size_t pos,
			 const char *fmt, ...)
{
	va_list ap;
	int n;

	if (pos >= bufsz - 1)
		return pos;
	va_start(ap, fmt);
	n = vsnprintf(buf + pos, bufsz - pos, fmt, ap);
	va_end(ap);
	if (n < 0)
		return pos;
	/* Clamp: snprintf returns the would-be length, which can
	 * exceed the remaining space. Cap at what fits.
	 */
	if ((size_t)n > bufsz - pos - 1)
		return bufsz - 1;
	return pos + (size_t)n;
}

static void test_large_realistic_config_parses(void)
{
	/* A config 10x the typical size -- simulates a heavy multi-
	 * function routing config. Budget = strlen * 1000 should
	 * comfortably accommodate this.
	 */
	static char big_config[8192];
	size_t pos = 0;
	int i;

	pos = append_chk(big_config, sizeof(big_config), pos,
		"{\"intercept_scripts\":[");
	for (i = 0; i < 50; i++) {
		pos = append_chk(big_config, sizeof(big_config), pos,
			"%s{\"func_name\":\"func_%d\",\"actions\":["
			"{\"action_name\":\"log_params\"},"
			"{\"action_name\":\"call_real\"}]}",
			i > 0 ? "," : "", i);
	}
	pos = append_chk(big_config, sizeof(big_config), pos, "]}");

	{
		JSON_Value *v = json_parse_string_with_comments(big_config);

		CHECK(v != NULL);
		CHECK(json_value_get_type(v) == JSONObject);
		json_value_free(v);
	}
}

int main(void)
{
	printf("-- realistic configs still parse --\n");
	TEST(realistic_config_parses);
	TEST(config_with_comments_parses);
	TEST(minimal_object_parses);
	TEST(minimal_array_parses);

	printf("-- budget resets between calls --\n");
	TEST(repeated_parse_does_not_accumulate);

	printf("-- edge cases don't crash --\n");
	TEST(null_input_returns_null);
	TEST(empty_string_returns_null);
	TEST(whitespace_only_returns_null);
	TEST(malformed_returns_null);

	printf("-- budget is generous enough --\n");
	TEST(large_realistic_config_parses);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
