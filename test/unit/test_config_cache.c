/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the config_cache module (TODO.complete/18).
 *
 * Verifies the cache correctly pre-resolves exact-name scripts
 * from the JSON config, skips wildcards, and handles edge cases.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config_cache.h"
#include "real_impls.h"
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

static JSON_Object *parse_conf(const char *json_str)
{
	JSON_Value *v = json_parse_string(json_str);

	return v ? json_value_get_object(v) : NULL;
}

static void test_empty_conf(void)
{
	JSON_Object *conf = parse_conf("{\"intercept_scripts\":[]}");

	retrace_config_cache_clear();
	assert(retrace_config_cache_build(conf) == 0);
	assert(retrace_config_cache_count() == 0);
}

static void test_single_exact(void)
{
	JSON_Object *conf = parse_conf(
		"{\"intercept_scripts\":[{\"func_name\":\"open\"}]}");

	retrace_config_cache_clear();
	assert(retrace_config_cache_build(conf) == 0);
	assert(retrace_config_cache_count() == 1);
	assert(retrace_config_cache_lookup("open") != NULL);
}

static void test_wildcard_skipped(void)
{
	JSON_Object *conf = parse_conf(
		"{\"intercept_scripts\":[{\"func_name\":\"*\"}]}");

	retrace_config_cache_clear();
	assert(retrace_config_cache_build(conf) == 0);
	assert(retrace_config_cache_count() == 0);
	assert(retrace_config_cache_lookup("*") == NULL);
}

static void test_mixed_entries(void)
{
	JSON_Object *conf = parse_conf(
		"{\"intercept_scripts\":["
		"{\"func_name\":\"open\"},"
		"{\"func_name\":\"*\"},"
		"{\"func_name\":\"malloc\"}"
		"]}");

	retrace_config_cache_clear();
	assert(retrace_config_cache_build(conf) == 0);
	assert(retrace_config_cache_count() == 2);
	assert(retrace_config_cache_lookup("open") != NULL);
	assert(retrace_config_cache_lookup("malloc") != NULL);
	assert(retrace_config_cache_lookup("*") == NULL);
}

static void test_miss(void)
{
	JSON_Object *conf = parse_conf(
		"{\"intercept_scripts\":[{\"func_name\":\"open\"}]}");

	retrace_config_cache_clear();
	retrace_config_cache_build(conf);
	assert(retrace_config_cache_lookup("nonexistent") == NULL);
}

static void test_null_inputs(void)
{
	assert(retrace_config_cache_build(NULL) == -1);
	assert(retrace_config_cache_lookup(NULL) == NULL);
}

static void test_no_scripts_array(void)
{
	JSON_Object *conf = parse_conf("{\"other\":\"value\"}");

	retrace_config_cache_clear();
	assert(retrace_config_cache_build(conf) == -1);
}

static void test_repeated_build_clears(void)
{
	JSON_Object *conf1 = parse_conf(
		"{\"intercept_scripts\":[{\"func_name\":\"foo\"}]}");
	JSON_Object *conf2 = parse_conf(
		"{\"intercept_scripts\":[{\"func_name\":\"bar\"}]}");

	retrace_config_cache_build(conf1);
	assert(retrace_config_cache_count() == 1);
	assert(retrace_config_cache_lookup("foo") != NULL);

	retrace_config_cache_build(conf2);
	assert(retrace_config_cache_count() == 1);
	assert(retrace_config_cache_lookup("foo") == NULL);
	assert(retrace_config_cache_lookup("bar") != NULL);
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

	printf("config_cache tests:\n");

	printf("  -- build + lookup --\n");
	TEST(empty_conf);
	TEST(single_exact);
	TEST(wildcard_skipped);
	TEST(mixed_entries);
	TEST(miss);

	printf("  -- edge cases --\n");
	TEST(null_inputs);
	TEST(no_scripts_array);
	TEST(repeated_build_clears);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
