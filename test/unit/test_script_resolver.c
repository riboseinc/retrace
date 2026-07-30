/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the script_resolver module (extracted from engine.c
 * per ADR-0013). Verifies the JSON intercept_script lookup semantics:
 * exact name match, wildcard "*", return_addr disambiguation, and
 * the "first name-only match wins" fallback.
 *
 * Issue #481.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "script_resolver.h"
#include "real_impls.h"
#include "logger.h"

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

/* Build a JSON array of intercept_scripts from a C string. */
static JSON_Array *parse_scripts(const char *json_str)
{
	char buf[1024];
	JSON_Value *root;
	JSON_Object *obj;
	JSON_Array *arr;

	snprintf(buf, sizeof(buf),
		"{\"intercept_scripts\": %s}", json_str);
	root = json_parse_string(buf);
	assert(root != NULL);
	obj = json_value_get_object(root);
	arr = json_object_get_array(obj, "intercept_scripts");
	assert(arr != NULL);
	return arr;
}

static void test_exact_name_match(void)
{
	JSON_Array *arr = parse_scripts(
		"[{\"func_name\":\"getuid\",\"actions\":[]}]");
	const JSON_Object *hit = retrace_script_find(arr, "getuid", NULL);

	assert(hit != NULL);
	assert(strcmp(json_object_get_string(hit, "func_name"), "getuid") == 0);
}

static void test_no_match_returns_null(void)
{
	JSON_Array *arr = parse_scripts(
		"[{\"func_name\":\"getuid\",\"actions\":[]}]");
	const JSON_Object *hit = retrace_script_find(arr, "malloc", NULL);

	assert(hit == NULL);
}

static void test_wildcard_matches_anything(void)
{
	JSON_Array *arr = parse_scripts(
		"[{\"func_name\":\"*\",\"actions\":[]}]");
	const JSON_Object *hit = retrace_script_find(arr, "getuid", NULL);

	assert(hit != NULL);
}

static void test_wildcard_does_not_shadow_exact(void)
{
	/* Exact match comes first in the array; the resolver returns it. */
	JSON_Array *arr = parse_scripts(
		"[{\"func_name\":\"getuid\",\"actions\":[]},"
		" {\"func_name\":\"*\",\"actions\":[]}]");
	const JSON_Object *hit = retrace_script_find(arr, "getuid", NULL);

	assert(hit != NULL);
	assert(strcmp(json_object_get_string(hit, "func_name"), "getuid") == 0);
}

static void test_first_name_only_match_wins(void)
{
	/* Two name-only matches for the same func; first wins. */
	JSON_Array *arr = parse_scripts(
		"[{\"func_name\":\"getuid\",\"actions\":[{\"action_name\":\"call_real\"}]},"
		" {\"func_name\":\"getuid\",\"actions\":[{\"action_name\":\"log_params\"}]}]");
	const JSON_Object *hit = retrace_script_find(arr, "getuid", NULL);
	const JSON_Array *actions;
	const JSON_Object *first_action;

	assert(hit != NULL);
	actions = json_object_get_array(hit, "actions");
	assert(actions != NULL);
	first_action = json_array_get_object(actions, 0);
	assert(strcmp(json_object_get_string(first_action, "action_name"),
		"call_real") == 0);
}

static void test_return_addr_specific_wins_over_name_only(void)
{
	/*
	 * name-only match appears first; name+return_addr appears second.
	 * Resolver returns the specific one even though it's later in the
	 * array.
	 */
	JSON_Array *arr = parse_scripts(
		"[{\"func_name\":\"open\",\"actions\":[]},"
		" {\"func_name\":\"open\",\"return_addr\":4096,\"actions\":[]}]");
	const JSON_Object *hit = retrace_script_find(arr, "open", (void *)4096);

	assert(hit != NULL);
	assert((long long)json_object_get_number(hit, "return_addr") == 4096);
}

static void test_return_addr_mismatch_falls_back_to_name_only(void)
{
	/* The specific entry's return_addr doesn't match; name-only wins. */
	JSON_Array *arr = parse_scripts(
		"[{\"func_name\":\"open\",\"actions\":[]},"
		" {\"func_name\":\"open\",\"return_addr\":9999,\"actions\":[]}]");
	const JSON_Object *hit = retrace_script_find(arr, "open", (void *)4096);

	assert(hit != NULL);
	/* Should be the first (name-only) entry. */
	assert(json_object_get_number(hit, "return_addr") == 0);
}

static void test_missing_func_name_skipped(void)
{
	/* An entry without func_name is logged but skipped. */
	JSON_Array *arr = parse_scripts(
		"[{\"actions\":[]},"
		" {\"func_name\":\"getuid\",\"actions\":[]}]");
	const JSON_Object *hit = retrace_script_find(arr, "getuid", NULL);

	assert(hit != NULL);
	assert(strcmp(json_object_get_string(hit, "func_name"), "getuid") == 0);
}

static void test_empty_array_returns_null(void)
{
	JSON_Array *arr = parse_scripts("[]");
	const JSON_Object *hit = retrace_script_find(arr, "anything", NULL);

	assert(hit == NULL);
}

int main(void)
{
	/* Wire real_impls to the real libc (no interposition in unit tests). */
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;

	printf("script_resolver:\n");
	TEST(exact_name_match);
	TEST(no_match_returns_null);
	TEST(wildcard_matches_anything);
	TEST(wildcard_does_not_shadow_exact);
	TEST(first_name_only_match_wins);
	TEST(return_addr_specific_wins_over_name_only);
	TEST(return_addr_mismatch_falls_back_to_name_only);
	TEST(missing_func_name_skipped);
	TEST(empty_array_returns_null);

	printf("\n%d run, %d passed, %d failed\n",
		tests_run, tests_pass, tests_fail);
	return tests_fail ? 1 : 0;
}
