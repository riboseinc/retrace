/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the audit policy module (TODO.complete/26).
 *
 * Covers:
 *   - severity_str round-trip across all enum values
 *   - policy_load_from_json parses every predicate type + severity
 *   - policy_load_from_json tolerates missing rules array
 *   - policy_load_from_json NULL safety
 *   - policy_free idempotency
 *   - policy_rule_matches across every predicate type:
 *       func_exact, func_prefix, path_contains, env_pattern
 *       (suffix, prefix, exact)
 *   - policy_rule_matches AND semantics (all non-NULL constraints
 *     must match)
 *   - policy_rule_matches NULL safety
 *
 * Note: function calls live OUTSIDE assert() so the side-effecting
 * call still happens under -DNDEBUG. Otherwise the test would
 * operate on uninitialized memory.
 */

#include "parson.h"
#include "policy.h"

#include <assert.h>
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

/* Always-on check. assert() alone is compiled out by NDEBUG. */
#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

/* ----- Helpers ----- */

static JSON_Object *msg_from_func(const char *func)
{
	JSON_Value *v = json_value_init_object();
	JSON_Object *o = json_value_get_object(v);

	if (func != NULL)
		json_object_set_string(o, "func", func);
	return o;
}

static JSON_Object *msg_with_string(const char *key, const char *val)
{
	JSON_Value *v = json_value_init_object();
	JSON_Object *o = json_value_get_object(v);

	json_object_set_string(o, key, val);
	return o;
}

static void msg_free(JSON_Object *o)
{
	json_value_free(json_object_get_wrapping_value(o));
}

/* ----- severity_str round-trip ----- */

static void test_severity_str_round_trip(void)
{
	CHECK(strcmp(severity_str(SEV_INFO), "info") == 0);
	CHECK(strcmp(severity_str(SEV_MEDIUM), "medium") == 0);
	CHECK(strcmp(severity_str(SEV_HIGH), "high") == 0);
	CHECK(strcmp(severity_str(SEV_CRITICAL), "critical") == 0);
}

static void test_severity_str_default_is_info(void)
{
	CHECK(strcmp(severity_str((enum Severity)99), "info") == 0);
}

/* ----- policy_load_from_json ----- */

static void test_load_all_predicate_types(void)
{
	const char *src =
		"{\"name\":\"test\", \"rules\":["
		"  {\"id\":\"R1\",\"description\":\"d1\","
		"   \"match\":{\"func_exact\":\"system\"},\"severity\":\"high\"},"
		"  {\"id\":\"R2\",\"description\":\"d2\","
		"   \"match\":{\"func_prefix\":\"get\"},\"severity\":\"medium\"},"
		"  {\"id\":\"R3\",\"description\":\"d3\","
		"   \"match\":{\"path_contains\":\"/etc/passwd\"},"
		"   \"severity\":\"critical\"},"
		"  {\"id\":\"R4\",\"description\":\"d4\","
		"   \"match\":{\"env_pattern\":\"*_TOKEN\"},"
		"   \"severity\":\"info\"}"
		"]}";
	JSON_Value *v = json_parse_string(src);
	JSON_Object *root = json_value_get_object(v);
	struct Policy p;
	int rc;

	rc = policy_load_from_json(root, &p);
	CHECK(rc == 0);
	CHECK(strcmp(p.name, "test") == 0);
	CHECK(p.rules_count == 4);

	CHECK(strcmp(p.rules[0].id, "R1") == 0);
	CHECK(p.rules[0].severity == SEV_HIGH);
	CHECK(p.rules[0].func_exact != NULL);
	CHECK(strcmp(p.rules[0].func_exact, "system") == 0);

	CHECK(strcmp(p.rules[1].id, "R2") == 0);
	CHECK(p.rules[1].severity == SEV_MEDIUM);
	CHECK(p.rules[1].func_prefix != NULL);
	CHECK(strcmp(p.rules[1].func_prefix, "get") == 0);

	CHECK(strcmp(p.rules[2].id, "R3") == 0);
	CHECK(p.rules[2].severity == SEV_CRITICAL);
	CHECK(p.rules[2].path_contains != NULL);
	CHECK(strcmp(p.rules[2].path_contains, "/etc/passwd") == 0);

	CHECK(strcmp(p.rules[3].id, "R4") == 0);
	CHECK(p.rules[3].severity == SEV_INFO);
	CHECK(p.rules[3].env_pattern != NULL);
	CHECK(strcmp(p.rules[3].env_pattern, "*_TOKEN") == 0);

	policy_free(&p);
	json_value_free(v);
}

static void test_load_missing_rules_array(void)
{
	const char *src = "{\"name\":\"empty\"}";
	JSON_Value *v = json_parse_string(src);
	JSON_Object *root = json_value_get_object(v);
	struct Policy p;
	int rc;

	rc = policy_load_from_json(root, &p);
	CHECK(rc == 0);
	CHECK(strcmp(p.name, "empty") == 0);
	CHECK(p.rules == NULL);
	CHECK(p.rules_count == 0);
	policy_free(&p);
	json_value_free(v);
}

static void test_load_unnamed_policy_uses_default(void)
{
	const char *src = "{}";
	JSON_Value *v = json_parse_string(src);
	JSON_Object *root = json_value_get_object(v);
	struct Policy p;
	int rc;

	rc = policy_load_from_json(root, &p);
	CHECK(rc == 0);
	CHECK(strcmp(p.name, "(unnamed)") == 0);
	policy_free(&p);
	json_value_free(v);
}

static void test_load_null_safety(void)
{
	struct Policy p;

	CHECK(policy_load_from_json(NULL, &p) == -1);
	CHECK(policy_load_from_json(NULL, NULL) == -1);
}

static void test_free_null_is_safe(void)
{
	policy_free(NULL);
}

/* ----- policy_rule_matches: func_exact ----- */

static void test_func_exact_match(void)
{
	struct Rule r = {"R", "", .func_exact = "system"};
	JSON_Object *m = msg_from_func("system");

	CHECK(policy_rule_matches(&r, m) == 1);
	msg_free(m);
}

static void test_func_exact_no_match(void)
{
	struct Rule r = {"R", "", .func_exact = "system"};
	JSON_Object *m = msg_from_func("execve");

	CHECK(policy_rule_matches(&r, m) == 0);
	msg_free(m);
}

static void test_func_exact_no_func_in_msg(void)
{
	struct Rule r = {"R", "", .func_exact = "system"};
	JSON_Object *m = msg_with_string("text", "no func here");

	CHECK(policy_rule_matches(&r, m) == 0);
	msg_free(m);
}

/* ----- policy_rule_matches: func_prefix ----- */

static void test_func_prefix_match(void)
{
	struct Rule r = {"R", "", .func_prefix = "get"};
	JSON_Object *m = msg_from_func("getenv");

	CHECK(policy_rule_matches(&r, m) == 1);
	msg_free(m);
}

static void test_func_prefix_no_match(void)
{
	struct Rule r = {"R", "", .func_prefix = "get"};
	JSON_Object *m = msg_from_func("setenv");

	CHECK(policy_rule_matches(&r, m) == 0);
	msg_free(m);
}

/* ----- policy_rule_matches: path_contains ----- */

static void test_path_contains_match_in_path(void)
{
	struct Rule r = {"R", "", .path_contains = "/etc/passwd"};
	JSON_Object *m = msg_with_string("path", "/etc/passwd");

	CHECK(policy_rule_matches(&r, m) == 1);
	msg_free(m);
}

static void test_path_contains_match_in_any_string_field(void)
{
	struct Rule r = {"R", "", .path_contains = "secret"};
	JSON_Object *m = msg_with_string("buf", "load the secret sauce");

	CHECK(policy_rule_matches(&r, m) == 1);
	msg_free(m);
}

static void test_path_contains_no_match(void)
{
	struct Rule r = {"R", "", .path_contains = "/etc/shadow"};
	JSON_Object *m = msg_with_string("path", "/etc/passwd");

	CHECK(policy_rule_matches(&r, m) == 0);
	msg_free(m);
}

/* ----- policy_rule_matches: env_pattern ----- */

static void test_env_pattern_suffix_match(void)
{
	struct Rule r = {"R", "", .env_pattern = "*_TOKEN"};
	JSON_Object *m = msg_with_string("name", "API_TOKEN");

	CHECK(policy_rule_matches(&r, m) == 1);
	msg_free(m);
}

static void test_env_pattern_suffix_no_match(void)
{
	struct Rule r = {"R", "", .env_pattern = "*_TOKEN"};
	JSON_Object *m = msg_with_string("name", "API_KEY");

	CHECK(policy_rule_matches(&r, m) == 0);
	msg_free(m);
}

static void test_env_pattern_prefix_match(void)
{
	struct Rule r = {"R", "", .env_pattern = "LD_*"};
	JSON_Object *m = msg_with_string("name", "LD_PRELOAD");

	CHECK(policy_rule_matches(&r, m) == 1);
	msg_free(m);
}

static void test_env_pattern_prefix_no_match(void)
{
	struct Rule r = {"R", "", .env_pattern = "LD_*"};
	JSON_Object *m = msg_with_string("name", "PATH");

	CHECK(policy_rule_matches(&r, m) == 0);
	msg_free(m);
}

static void test_env_pattern_exact_match(void)
{
	struct Rule r = {"R", "", .env_pattern = "IFS"};
	JSON_Object *m = msg_with_string("name", "IFS");

	CHECK(policy_rule_matches(&r, m) == 1);
	msg_free(m);
}

static void test_env_pattern_null_name_in_msg(void)
{
	struct Rule r = {"R", "", .env_pattern = "*_TOKEN"};
	JSON_Object *m = msg_from_func("getenv");  /* no "name" field */

	CHECK(policy_rule_matches(&r, m) == 0);
	msg_free(m);
}

/* ----- policy_rule_matches: AND semantics ----- */

static void test_and_both_match(void)
{
	struct Rule r = {"R", "", .func_exact = "open",
		.path_contains = "/etc"};
	JSON_Object *m = msg_from_func("open");

	json_object_set_string(m, "path", "/etc/passwd");
	CHECK(policy_rule_matches(&r, m) == 1);
	msg_free(m);
}

static void test_and_one_fails(void)
{
	struct Rule r = {"R", "", .func_exact = "open",
		.path_contains = "/etc"};
	JSON_Object *m = msg_from_func("open");

	json_object_set_string(m, "path", "/var/log/foo");
	CHECK(policy_rule_matches(&r, m) == 0);
	msg_free(m);
}

/* ----- policy_rule_matches: edge cases ----- */

static void test_empty_rule_matches_anything(void)
{
	struct Rule r = {"R", ""};  /* all predicates NULL */
	JSON_Object *m = msg_from_func("anything");

	CHECK(policy_rule_matches(&r, m) == 1);
	msg_free(m);
}

static void test_null_inputs_safe(void)
{
	struct Rule r = {"R", ""};
	JSON_Object *m = msg_from_func("x");

	CHECK(policy_rule_matches(NULL, m) == 0);
	CHECK(policy_rule_matches(&r, NULL) == 0);
	msg_free(m);
}

int main(void)
{
	printf("-- severity_str --\n");
	TEST(severity_str_round_trip);
	TEST(severity_str_default_is_info);

	printf("-- policy_load_from_json --\n");
	TEST(load_all_predicate_types);
	TEST(load_missing_rules_array);
	TEST(load_unnamed_policy_uses_default);
	TEST(load_null_safety);
	TEST(free_null_is_safe);

	printf("-- policy_rule_matches: func_exact --\n");
	TEST(func_exact_match);
	TEST(func_exact_no_match);
	TEST(func_exact_no_func_in_msg);

	printf("-- policy_rule_matches: func_prefix --\n");
	TEST(func_prefix_match);
	TEST(func_prefix_no_match);

	printf("-- policy_rule_matches: path_contains --\n");
	TEST(path_contains_match_in_path);
	TEST(path_contains_match_in_any_string_field);
	TEST(path_contains_no_match);

	printf("-- policy_rule_matches: env_pattern --\n");
	TEST(env_pattern_suffix_match);
	TEST(env_pattern_suffix_no_match);
	TEST(env_pattern_prefix_match);
	TEST(env_pattern_prefix_no_match);
	TEST(env_pattern_exact_match);
	TEST(env_pattern_null_name_in_msg);

	printf("-- policy_rule_matches: AND semantics --\n");
	TEST(and_both_match);
	TEST(and_one_fails);

	printf("-- policy_rule_matches: edge cases --\n");
	TEST(empty_rule_matches_anything);
	TEST(null_inputs_safe);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
