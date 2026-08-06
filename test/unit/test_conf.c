/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the JSON config loader (src/config/json/conf.c).
 *
 * retrace_conf_init() reads RETRACE_JSON_CONFIG env var, opens
 * the file, parses it via parson-with-comments, and stores the
 * root JSON_Object in the global retrace_conf. If the env var
 * is unset or the file can't be opened, falls back to the
 * built-in default config (log_params + call_real for "*").
 *
 * Tests:
 *   - default_config_used_when_no_env_var
 *   - valid_file_with_one_script
 *   - valid_file_with_multiple_scripts
 *   - valid_file_with_comments
 *   - malformed_file_returns_error
 *   - missing_file_falls_back_to_default
 *   - empty_intercept_scripts_array
 *
 * Setup: each test writes a temp file to /tmp, sets the env var,
 * calls retrace_conf_init, asserts on retrace_conf, cleans up.
 *
 * Part of TODO.complete/14. Closes the last open item (json
 * config loader) per the TODO doc.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "conf.h"
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

#define TEST_FILE "/tmp/retrace_conf_test.json"

static void write_file(const char *path, const char *content)
{
	FILE *f = fopen(path, "w");

	assert(f != NULL);
	fputs(content, f);
	fclose(f);
}

static void remove_file(const char *path)
{
	unlink(path);
}

static void unset_env(void)
{
	unsetenv("RETRACE_JSON_CONFIG");
}

static void set_env(const char *path)
{
	setenv("RETRACE_JSON_CONFIG", path, 1);
}

static void test_default_config_used_when_no_env_var(void)
{
	unset_env();
	assert(retrace_conf_init() == 0);
	assert(retrace_conf != NULL);

	/* Default config has intercept_scripts[0].func_name == "*" */
	{
		JSON_Array *scripts = json_object_get_array(retrace_conf,
			"intercept_scripts");

		assert(scripts != NULL);
		assert(json_array_get_count(scripts) == 1);
		assert(strcmp(json_object_get_string(
			json_array_get_object(scripts, 0), "func_name"),
			"*") == 0);
	}
}

static void test_valid_file_with_one_script(void)
{
	const char *json =
		"{\"intercept_scripts\": ["
		"  {\"func_name\": \"open\","
		"   \"actions\": ["
		"     {\"action_name\": \"log_params\"},"
		"     {\"action_name\": \"call_real\"}"
		"   ]}"
		"]}";

	write_file(TEST_FILE, json);
	set_env(TEST_FILE);

	assert(retrace_conf_init() == 0);

	{
		JSON_Array *scripts = json_object_get_array(retrace_conf,
			"intercept_scripts");

		assert(scripts != NULL);
		assert(json_array_get_count(scripts) == 1);
		assert(strcmp(json_object_get_string(
			json_array_get_object(scripts, 0), "func_name"),
			"open") == 0);

		{
			JSON_Array *actions = json_object_get_array(
				json_array_get_object(scripts, 0), "actions");

			assert(actions != NULL);
			assert(json_array_get_count(actions) == 2);
		}
	}

	remove_file(TEST_FILE);
	unset_env();
}

static void test_valid_file_with_multiple_scripts(void)
{
	const char *json =
		"{\"intercept_scripts\": ["
		"  {\"func_name\": \"open\","
		"   \"actions\": [{\"action_name\": \"log_params\"}]},"
		"  {\"func_name\": \"malloc\","
		"   \"actions\": [{\"action_name\": \"log_params\"},"
		"                 {\"action_name\": \"call_real\"}]}"
		"]}";

	write_file(TEST_FILE, json);
	set_env(TEST_FILE);

	assert(retrace_conf_init() == 0);

	{
		JSON_Array *scripts = json_object_get_array(retrace_conf,
			"intercept_scripts");

		assert(json_array_get_count(scripts) == 2);
		assert(strcmp(json_object_get_string(
			json_array_get_object(scripts, 0), "func_name"),
			"open") == 0);
		assert(strcmp(json_object_get_string(
			json_array_get_object(scripts, 1), "func_name"),
			"malloc") == 0);
	}

	remove_file(TEST_FILE);
	unset_env();
}

static void test_valid_file_with_comments(void)
{
	/* parson's comment-tolerant parser must accept // and /* */
	const char *json =
		"// top-level comment\n"
		"{\n"
		"  \"intercept_scripts\": [\n"
		"    /* script for open */\n"
		"    {\"func_name\": \"open\",\n"
		"     \"actions\": [{\"action_name\": \"log_params\"}]}\n"
		"  ]\n"
		"}\n";

	write_file(TEST_FILE, json);
	set_env(TEST_FILE);

	assert(retrace_conf_init() == 0);

	{
		JSON_Array *scripts = json_object_get_array(retrace_conf,
			"intercept_scripts");

		assert(json_array_get_count(scripts) == 1);
	}

	remove_file(TEST_FILE);
	unset_env();
}

static void test_malformed_file_returns_error(void)
{
	const char *json = "{ this is not valid json";

	write_file(TEST_FILE, json);
	set_env(TEST_FILE);

	/* retrace_conf_init returns -1 on parse failure. */
	assert(retrace_conf_init() == -1);

	remove_file(TEST_FILE);
	unset_env();
}

static void test_missing_file_falls_back_to_default(void)
{
	/* Point to a path that doesn't exist; init should fall back
	 * to the default config (returns 0, retrace_conf populated).
	 */
	set_env("/tmp/retrace_nonexistent_file_12345.json");

	assert(retrace_conf_init() == 0);

	{
		JSON_Array *scripts = json_object_get_array(retrace_conf,
			"intercept_scripts");

		assert(scripts != NULL);
		/* Default config: 1 wildcard script. */
		assert(json_array_get_count(scripts) == 1);
		assert(strcmp(json_object_get_string(
			json_array_get_object(scripts, 0), "func_name"),
			"*") == 0);
	}

	unset_env();
}

static void test_empty_intercept_scripts_array(void)
{
	const char *json = "{\"intercept_scripts\": []}";

	write_file(TEST_FILE, json);
	set_env(TEST_FILE);

	assert(retrace_conf_init() == 0);

	{
		JSON_Array *scripts = json_object_get_array(retrace_conf,
			"intercept_scripts");

		assert(scripts != NULL);
		assert(json_array_get_count(scripts) == 0);
	}

	remove_file(TEST_FILE);
	unset_env();
}

int main(void)
{
	/* conf.c uses retrace_real_impls for fopen/fread/etc. */
	retrace_real_impls.fopen = fopen;
	retrace_real_impls.fclose = fclose;
	retrace_real_impls.fread = fread;
	retrace_real_impls.fseek = fseek;
	retrace_real_impls.ftell = ftell;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.getenv = getenv;
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.real_snprintf = snprintf;

	printf("json config loader tests:\n");

	printf("  -- default config --\n");
	TEST(default_config_used_when_no_env_var);
	TEST(missing_file_falls_back_to_default);

	printf("  -- file parsing --\n");
	TEST(valid_file_with_one_script);
	TEST(valid_file_with_multiple_scripts);
	TEST(valid_file_with_comments);
	TEST(empty_intercept_scripts_array);

	printf("  -- error paths --\n");
	TEST(malformed_file_returns_error);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
