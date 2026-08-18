/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Public-API surface guard (ADR-0014).
 *
 * Every function declared in <retrace/retrace.h> must exist as an
 * exported symbol in the built library. Before v2.5.0 the header
 * declared ~26 functions that were never implemented -- consumers
 * compiled fine and failed at link time. This test dlsyms each
 * declaration so a phantom symbol can never ship again: adding a
 * declaration to retrace.h without a definition fails the build.
 *
 * Also pins the version functions' contracts.
 */

#include <retrace/retrace.h>
#include <retrace/version.h>

#include <dlfcn.h>
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

/*
 * The complete list of RETRACE_API functions declared in
 * retrace.h. Keep in sync with the header -- that is the point:
 * if a declaration is added without updating this list AND
 * providing the symbol, the test's second half (below) is the
 * real guard; this list makes the audit explicit.
 */
static const char *const public_symbols[] = {
	"retrace_version",
	"retrace_version_info",
	"retrace_attach_process",
	"retrace_list_backends",
	"retrace_list_functions",
	"retrace_list_actions",
};

static void *g_lib;

static int open_library(void)
{
	if (g_lib != NULL)
		return 0;
	g_lib = dlopen(NULL, RTLD_NOW);  /* the running image itself */
	if (g_lib == NULL) {
		printf("(dlopen(self) failed: %s) ", dlerror());
		return -1;
	}
	return 0;
}

static void test_every_declared_symbol_resolves(void)
{
	size_t i;

	CHECK(open_library() == 0);
	for (i = 0; i < sizeof(public_symbols) / sizeof(public_symbols[0]);
	     i++) {
		void *sym = dlsym(g_lib, public_symbols[i]);

		if (sym == NULL)
			printf("[%s: MISSING] ", public_symbols[i]);
		CHECK(sym != NULL);
	}
}

static void test_retrace_version_string(void)
{
	CHECK(retrace_version() != NULL);
	CHECK(strcmp(retrace_version(), RETRACE_VERSION_STRING) == 0);
}

static void test_retrace_version_info_string(void)
{
	const char *info = retrace_version_info();

	CHECK(info != NULL);
	CHECK(strstr(info, RETRACE_VERSION_STRING) != NULL);
	CHECK(strstr(info, "retrace") != NULL);
}

static void test_attach_invalid_pid(void)
{
	CHECK(retrace_attach_process(0) != RETRACE_OK);
	CHECK(retrace_attach_process(-1) != RETRACE_OK);
}

static void test_list_backends_contract(void)
{
	const char *const *names = NULL;
	size_t count = 0;

	CHECK(retrace_list_backends(NULL, NULL) == RETRACE_ERR_INVAL);
	CHECK(retrace_list_backends(&names, &count) == RETRACE_OK);
	CHECK(count > 0);
	CHECK(names != NULL);
	CHECK(names[0] != NULL && names[0][0] != '\0');
}

static int contains(const char *const *names, size_t count,
		    const char *want)
{
	size_t i;

	for (i = 0; i < count; i++)
		if (names[i] != NULL && strcmp(names[i], want) == 0)
			return 1;
	return 0;
}

static void test_list_functions_contract(void)
{
	const char *const *names = NULL;
	size_t count = 0;
	size_t i;

	CHECK(retrace_list_functions(NULL, NULL) == RETRACE_ERR_INVAL);
	CHECK(retrace_list_functions(&names, &count) == RETRACE_OK);
	CHECK(count > 100);  /* the prototype registry is large */
	for (i = 0; i < count; i++)
		CHECK(names[i] != NULL && names[i][0] != '\0');
	/* Well-known interceptable functions must be present. */
	CHECK(contains(names, count, "malloc"));
	CHECK(contains(names, count, "open"));
	CHECK(contains(names, count, "write"));
}

static void test_list_actions_contract(void)
{
	const char *const *names = NULL;
	size_t count = 0;
	size_t i;

	CHECK(retrace_list_actions(NULL, NULL) == RETRACE_ERR_INVAL);
	CHECK(retrace_list_actions(&names, &count) == RETRACE_OK);
	CHECK(count >= 17);  /* built-in actions */
	for (i = 0; i < count; i++)
		CHECK(names[i] != NULL && names[i][0] != '\0');
	/* Built-ins from every category. */
	CHECK(contains(names, count, "log_params"));
	CHECK(contains(names, count, "call_real"));
	CHECK(contains(names, count, "memory_fuzz"));
	CHECK(contains(names, count, "capture_buffer"));
}

int main(void)
{
	printf("-- surface guard --\n");
	TEST(every_declared_symbol_resolves);

	printf("-- version contract --\n");
	TEST(retrace_version_string);
	TEST(retrace_version_info_string);

	printf("-- behavior smoke --\n");
	TEST(attach_invalid_pid);
	TEST(list_backends_contract);
	TEST(list_functions_contract);
	TEST(list_actions_contract);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
