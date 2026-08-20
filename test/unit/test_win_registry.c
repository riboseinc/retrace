/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Windows PE-section registry tests (TODO.windows/05).
 *
 * The POSIX test_registries.c proves the __start_/_stop_ section
 * walk; this twin proves the PE path: role-tagged short sections
 * (.rtrA/.rtrF/.rtrD) found via the module's own PE headers
 * (win_common/section_walk.c). Runs on every Windows leg (x64
 * and arm64 alike -- the registry is arch-neutral).
 *
 * Also covers the ntdll prototype table and the
 * OBJECT_ATTRIBUTES/UNICODE_STRING decoders (TODO.windows/06).
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

#include "actions.h"
#include "data_types.h"
#include "funcs.h"
#include "real_impls.h"

#include "arch_spec_macros.h"

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

/* -- Registry walk (the PE sections resolve) -- */

static void test_funcs_registry(void)
{
	CHECK(retrace_func_get("fopen") != NULL);
	CHECK(retrace_func_get("malloc") != NULL);
	CHECK(retrace_func_get("strlen") != NULL);
	CHECK(retrace_func_get("no_such_function_xyz") == NULL);
}

static void test_ntdll_registry(void)
{
	const struct FuncPrototype *p;

	p = retrace_func_get("NtCreateFile");
	CHECK(p != NULL);
	CHECK(p->params_cnt == 11);
	CHECK(p->conv == CC_MICROSOFT);

	p = retrace_func_get("NtQueryAttributesFile");
	CHECK(p != NULL);
	CHECK(p->params_cnt == 2);

	p = retrace_func_get("NtClose");
	CHECK(p != NULL);
	CHECK(p->params_cnt == 1);

	p = retrace_func_get("LdrLoadDll");
	CHECK(p != NULL);
	CHECK(p->params_cnt == 4);
}

static void test_actions_registry(void)
{
	CHECK(retrace_actions_get("log_params") != NULL);
	CHECK(retrace_actions_get("call_real") != NULL);
	CHECK(retrace_actions_get("sandbox") != NULL);
	CHECK(retrace_actions_get("no_such_action_xyz") == NULL);
}

static void test_datatypes_registry(void)
{
	CHECK(retrace_datatype_get("sz") != NULL);
	CHECK(retrace_datatype_get("int") != NULL);
	CHECK(retrace_datatype_get("no_such_type_xyz") == NULL);
}

/* -- ntdll decoders (TODO.windows/06) -- */

/* Minimal OBJECT_ATTRIBUTES/UNICODE_STRING pair, x64 layout. */
struct test_unicode_string {
	unsigned short length;
	unsigned short maximum_length;
	unsigned int pad;
	wchar_t *buffer;
};

struct test_object_attributes {
	unsigned int length;
	unsigned int pad;
	void *root_directory;
	struct test_unicode_string *object_name;
	unsigned int attributes;
};

static void test_ntoa_decoder(void)
{
	const struct DataType *dt = retrace_datatype_get("ntoa");
	struct test_unicode_string us;
	struct test_object_attributes oa;
	char buf[256];
	size_t n;

	CHECK(dt != NULL);

	memset(&us, 0, sizeof(us));
	memset(&oa, 0, sizeof(oa));
	us.buffer = L"\\??\\C:\\vfs\\entry.dat";
	us.length = (unsigned short)(wcslen(us.buffer) * 2);
	us.maximum_length = us.length;
	oa.object_name = &us;
	oa.length = sizeof(oa);

	n = dt->to_sz(&oa, dt, buf);
	CHECK(n > 1);
	CHECK(strcmp(buf, "\\??\\C:\\vfs\\entry.dat") == 0);
}

static void test_ntoa_empty(void)
{
	const struct DataType *dt = retrace_datatype_get("ntoa");
	struct test_object_attributes oa;
	char buf[8];
	size_t n;

	CHECK(dt != NULL);
	memset(&oa, 0, sizeof(oa));
	oa.length = sizeof(oa);

	/* NULL ObjectName must decode to an empty string, not crash */
	n = dt->to_sz(&oa, dt, buf);
	CHECK(n == 1);
	CHECK(buf[0] == '\0');
}

static void test_ntus_decoder(void)
{
	const struct DataType *dt = retrace_datatype_get("ntus");
	struct test_unicode_string us;
	char buf[128];
	size_t n;

	CHECK(dt != NULL);
	memset(&us, 0, sizeof(us));
	us.buffer = L"sass.dll";
	us.length = (unsigned short)(wcslen(us.buffer) * 2);
	us.maximum_length = us.length;

	n = dt->to_sz(&us, dt, buf);
	CHECK(n > 1);
	CHECK(strcmp(buf, "sass.dll") == 0);
}

static void diag_section(const char *key, size_t elem_size)
{
	void *addr = NULL;
	unsigned long size = 0;

	if (retrace_win_section_lookup(key, &addr, &size)) {
		printf("diag %s: addr=%p size=%lu elems=%lu\n",
			key, addr, size, size / (unsigned long)elem_size);
	} else {
		printf("diag %s: NOT FOUND\n", key);
		retrace_win_dump_sections();
	}
}

int main(void)
{
	/* unbuffered: a crash must not swallow the diagnostics */
	setvbuf(stdout, NULL, _IONBF, 0);

	/* Minimal real_impls, same as test_registries.c (no
	 * interposition in unit tests).
	 */
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;
	retrace_real_impls.real_vsnprintf = vsnprintf;
	retrace_real_impls.getenv = getenv;

	diag_section("__DATA__retrace_funcs", sizeof(struct FuncPrototype));
	diag_section("__DATA__retrace_acts",
		sizeof(struct Action));
	diag_section("__DATA__retrace_dt",
		sizeof(struct DataType));

	printf("Initializing registries (PE section walk)...\n");
	{
		int fret = retrace_funcs_init();
		size_t k;

		printf("funcs_init rc=%d\n", fret);
		/*
		 * bisect the registration boundary: element index ->
		 * name (from the .rtrF layout, see the object dump)
		 */
		{
			static const char *const sentinels[] = {
				"setlocale", "fopen", "malloc", "bsearch",
				"qsort", "strdup", "accept", "bind",
				"strlen", "memcpy", "getpid", "write",
				"pthread_create", "time", "NtCreateFile",
				NULL
			};

			for (k = 0; sentinels[k] != NULL; k++)
				printf("probe %-20s %s\n", sentinels[k],
					retrace_func_get(sentinels[k]) ?
					"FOUND" : "MISSING");
		}
	}
	retrace_actions_init();
	retrace_datatypes_init();

	printf("Windows registry tests:\n");
	TEST(funcs_registry);
	TEST(ntdll_registry);
	TEST(actions_registry);
	TEST(datatypes_registry);

	printf("ntdll decoders:\n");
	TEST(ntoa_decoder);
	TEST(ntoa_empty);
	TEST(ntus_decoder);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
