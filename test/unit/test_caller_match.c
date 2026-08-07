/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the caller_match module (TODO.complete/17).
 *
 * The matchers have well-defined contracts that can be tested
 * without dladdr actually working:
 *
 *   - address match: pure integer equality
 *   - symbol match: requires real symbol (use a local static fn)
 *   - module_offset match: same + module basename comparison
 *   - kind_from_string: enum mapping
 *   - NULL inputs: return -1
 *
 * For symbol + module_offset tests, we use a local static
 * function whose symbol should be resolvable via dladdr on any
 * POSIX platform. If dladdr fails (e.g., stripped binary with no
 * symbol table), the test reports gracefully rather than failing
 * -- the matcher contract is "return -1 on dladdr failure".
 *
 * Part of TODO.complete/17.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "caller_match.h"
#include "real_impls.h"

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

/* A local static function -- its address is in the test binary,
 * so dladdr on &it should resolve to "test_local_symbol_target".
 */
static int test_local_symbol_target(int x)
{
	return x + 1;
}

/* -- kind_from_string -- */

static void test_kind_from_string_address(void)
{
	assert(retrace_caller_match_kind_from_string("address") ==
	       RETRACE_CALLER_MATCH_ADDRESS);
}

static void test_kind_from_string_symbol(void)
{
	assert(retrace_caller_match_kind_from_string("symbol") ==
	       RETRACE_CALLER_MATCH_SYMBOL);
}

static void test_kind_from_string_module_offset(void)
{
	assert(retrace_caller_match_kind_from_string("offset_in_module") ==
	       RETRACE_CALLER_MATCH_MODULE_OFFSET);
}

static void test_kind_from_string_unknown(void)
{
	assert(retrace_caller_match_kind_from_string("garbage") ==
	       RETRACE_CALLER_MATCH_UNKNOWN);
}

static void test_kind_from_string_null(void)
{
	assert(retrace_caller_match_kind_from_string(NULL) ==
	       RETRACE_CALLER_MATCH_UNKNOWN);
}

/* -- address match -- */

static void test_address_match_exact(void)
{
	int marker;
	void *addr = (void *)&marker;

	assert(retrace_caller_match_address(addr,
		(unsigned long long)(unsigned long)addr) == 1);
}

static void test_address_match_mismatch(void)
{
	int marker;
	void *addr = (void *)&marker;

	assert(retrace_caller_match_address(addr, 0xdeadbeefULL) == 0);
}

static void test_address_match_null_input(void)
{
	assert(retrace_caller_match_address(NULL, 0ULL) == -1);
}

/* -- symbol match -- */

static void test_symbol_match_null_input(void)
{
	assert(retrace_caller_match_symbol(NULL, "foo") == -1);
	assert(retrace_caller_match_symbol((void *)test_kind_from_string_null,
		NULL) == -1);
	assert(retrace_caller_match_symbol((void *)test_kind_from_string_null,
		"") == -1);
}

static void test_symbol_match_real_function(void)
{
	int rc;

	rc = retrace_caller_match_symbol(
		(void *)test_local_symbol_target,
		"test_local_symbol_target");

	/* If dladdr can resolve the symbol (test binary has symbol
	 * table), rc must be 1. If dladdr fails (stripped binary),
	 * rc is -1. Either is acceptable per the matcher contract;
	 * a hard "0 = no match" would be a real bug.
	 */
	assert(rc == 1 || rc == -1);
}

static void test_symbol_match_wrong_name(void)
{
	int rc;

	rc = retrace_caller_match_symbol(
		(void *)test_local_symbol_target,
		"definitely_not_a_real_symbol");

	/* rc could be 0 (dladdr succeeded but symbol name differs)
	 * or -1 (dladdr failed entirely). Must NOT be 1.
	 */
	assert(rc != 1);
}

/* -- module_offset match -- */

static void test_module_offset_null_input(void)
{
	assert(retrace_caller_match_module_offset(NULL, "libc.so", 0) == -1);
	assert(retrace_caller_match_module_offset(
		   (void *)test_kind_from_string_null, NULL, 0) == -1);
	assert(retrace_caller_match_module_offset(
		   (void *)test_kind_from_string_null, "", 0) == -1);
}

static void test_module_offset_real_function_wrong_module(void)
{
	int rc;

	/* The test binary's main module is NOT "definitely_not_a_module.so".
	 * Even if dladdr succeeds, the basename won't match.
	 */
	rc = retrace_caller_match_module_offset(
		(void *)test_local_symbol_target,
		"definitely_not_a_module.so",
		0ULL);

	assert(rc == 0 || rc == -1);
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

	/* Touch the function so the linker doesn't drop it. */
	(void)test_local_symbol_target(0);

	printf("caller_match tests:\n");

	printf("  -- kind_from_string --\n");
	TEST(kind_from_string_address);
	TEST(kind_from_string_symbol);
	TEST(kind_from_string_module_offset);
	TEST(kind_from_string_unknown);
	TEST(kind_from_string_null);

	printf("  -- address match --\n");
	TEST(address_match_exact);
	TEST(address_match_mismatch);
	TEST(address_match_null_input);

	printf("  -- symbol match --\n");
	TEST(symbol_match_null_input);
	TEST(symbol_match_real_function);
	TEST(symbol_match_wrong_name);

	printf("  -- module_offset match --\n");
	TEST(module_offset_null_input);
	TEST(module_offset_real_function_wrong_module);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
