/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Shared test utilities for retrace unit tests (TODO.complete/14).
 *
 * Extracts the common boilerplate duplicated across 20+ test files:
 *   - action_fn_t typedef (the function pointer type for actions)
 *   - TEST(name) macro (run + assert + count pattern)
 *   - JSON builder helpers (build_json_number, build_json_string,
 *     build_json_array)
 *
 * Usage:
 *   #include "test_utils.h"
 *
 *   static void test_foo(void) { ... }
 *
 *   int main(void) {
 *       retrace_real_impls.strcmp = strcmp;
 *       ...
 *       INIT_TESTS();
 *       TEST(foo);
 *       FINISH_TESTS();
 *   }
 *
 * Files using this header no longer need to re-declare action_fn_t,
 * re-define the TEST macro, or re-implement JSON builders.
 */

#ifndef RETRACE_TEST_UTILS_H_
#define RETRACE_TEST_UTILS_H_

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "engine.h"
#include "actions.h"
#include "funcs.h"
#include "data_types.h"
#include "real_impls.h"
#include "parson.h"

/* Function pointer type matching the Action callback signature.
 * checkpatch requires parameter names in function pointer
 * declarations; centralizing here avoids repeating the long
 * decl in every test file.
 */
typedef int (*action_fn_t)(struct ThreadContext *t_ctx,
			    const JSON_Object *action_params);

/* Test counter state. Declared once per test binary via
 * DECLARE_TEST_STATE() at file scope, used by TEST() and
 * FINISH_TESTS().
 */
#define DECLARE_TEST_STATE() \
	static int tests_run; \
	static int tests_pass; \
	static int tests_fail

#define INIT_TESTS() \
	do { tests_run = 0; tests_pass = 0; tests_fail = 0; } while (0)

/* Run a named test function (test_<name>). Prints progress.
 * On assert failure, the process aborts (standard C assert).
 */
#define TEST(name) do { \
	tests_run++; \
	printf("  TEST %s ... ", #name); \
	test_##name(); \
	tests_pass++; \
	printf("OK\n"); \
} while (0)

/* Print summary and return exit code. Pass the counters from
 * DECLARE_TEST_STATE(). Usage: return finish_tests(tests_run,
 * tests_pass, tests_fail);
 */
static inline int finish_tests(int run, int pass, int fail)
{
	printf("\nPass: %d, Fail: %d (of %d)\n", pass, fail, run);
	return fail == 0 ? 0 : 1;
}

/* --- JSON builder helpers --- */

/* Build a JSON object with one numeric key-value pair.
 * Caller owns the result; free via json_value_free(
 * json_object_get_wrapping_value(obj)).
 */
static inline JSON_Object *build_json_number(const char *key, double val)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_number(root, key, val);
	return root;
}

/* Build a JSON object with one string key-value pair. */
static inline JSON_Object *build_json_string_kv(const char *key,
						const char *val)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, key, val);
	return root;
}

/* Build a JSON object with one string array key-value pair.
 * vals is an array of N const char* strings.
 */
static inline JSON_Object *build_json_string_array(const char *key,
						   const char **vals, int n)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	JSON_Value *arr_val = json_value_init_array();
	JSON_Array *arr = json_array(arr_val);
	int i;

	for (i = 0; i < n; i++)
		json_array_append_string(arr, vals[i]);

	json_object_set_value(root, key, arr_val);
	return root;
}

/* Wire minimal retrace_real_impls for unit tests. Call once at
 * the top of main(). Maps all fields to libc directly (no
 * interposition in unit tests).
 */
static inline void init_minimal_real_impls(void)
{
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.strncmp = strncmp;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;
	retrace_real_impls.real_sprintf = sprintf;
	retrace_real_impls.real_vsnprintf = vsnprintf;
}

#endif /* RETRACE_TEST_UTILS_H_ */
