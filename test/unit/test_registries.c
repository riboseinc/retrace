/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the prototype, action, and data-type registries.
 * Links against retrace_core OBJECT library directly (no LD_PRELOAD).
 * Sets up minimal retrace_real_impls pointers for the init functions
 * to work, then tests registry lookups.
 *
 * Issue #481: establish the unit test layer.
 *
 * NOTE: we don't call retrace_as_get_section_info directly in this
 * file because the macro declares `extern char start_mysection`
 * with file-scope asm labels. Multiple calls from the same .c file
 * produce conflicting asm labels. The init functions (which are in
 * separate .c files) call it safely.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "funcs.h"
#include "actions.h"
#include "data_types.h"
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

/* -- Prototype registry -- */

static void test_proto_malloc(void)
{
	const struct FuncPrototype *p = retrace_func_get("malloc");

	assert(p != NULL);
	assert(strcmp(p->name, "malloc") == 0);
	assert(p->params_cnt == 1);
}

static void test_proto_printf_variadic(void)
{
	const struct FuncPrototype *p = retrace_func_get("printf");

	assert(p != NULL);
	assert(strcmp(p->name, "printf") == 0);
	assert(p->fmt == FAT_PRINTF);
}

static void test_proto_pthread_mutex_lock(void)
{
	const struct FuncPrototype *p = retrace_func_get("pthread_mutex_lock");

	assert(p != NULL);
	assert(strcmp(p->name, "pthread_mutex_lock") == 0);
}

static void test_proto_strlen(void)
{
	const struct FuncPrototype *p = retrace_func_get("strlen");

	assert(p != NULL);
	assert(strcmp(p->name, "strlen") == 0);
	assert(p->params_cnt == 1);
}

static void test_proto_nonexistent(void)
{
	assert(retrace_func_get("nonexistent_func_12345") == NULL);
}

/* Verify we have prototypes across multiple libc headers */
static void test_proto_cross_header(void)
{
	assert(retrace_func_get("read") != NULL);      /* unistd.h */
	assert(retrace_func_get("fopen") != NULL);     /* stdio.h */
	assert(retrace_func_get("malloc") != NULL);    /* stdlib.h */
	assert(retrace_func_get("strlen") != NULL);    /* string.h */
	assert(retrace_func_get("time") != NULL);      /* time.h */
	assert(retrace_func_get("isalpha") != NULL);   /* ctype.h */
	assert(retrace_func_get("opendir") != NULL);   /* dirent.h */
	assert(retrace_func_get("dlopen") != NULL);    /* unistd.h (dlfcn) */
}

/* -- Action registry -- */

static void test_action_log_params(void)
{
	assert(retrace_actions_get("log_params") != NULL);
}

static void test_action_call_real(void)
{
	assert(retrace_actions_get("call_real") != NULL);
}

static void test_action_modify_return_value(void)
{
	assert(retrace_actions_get("modify_return_value_int") != NULL);
}

static void test_action_incomplete_io(void)
{
	assert(retrace_actions_get("incomplete_io") != NULL);
}

static void test_action_fuzzing_seed(void)
{
	assert(retrace_actions_get("fuzzing_seed") != NULL);
}

static void test_action_memory_fuzz(void)
{
	assert(retrace_actions_get("memory_fuzz") != NULL);
}

static void test_action_nonexistent(void)
{
	assert(retrace_actions_get("nonexistent_action") == NULL);
}

/* -- Data type registry -- */

static void test_dt_int(void)
{
	const struct DataType *dt = retrace_datatype_get("int");

	assert(dt != NULL);
	assert(strcmp(dt->name, "int") == 0);
}

static void test_dt_sz(void)
{
	const struct DataType *dt = retrace_datatype_get("sz");

	assert(dt != NULL);
	assert(strcmp(dt->name, "sz") == 0);
}

static void test_dt_ptr(void)
{
	assert(retrace_datatype_get("ptr") != NULL);
}

static void test_dt_size_t(void)
{
	assert(retrace_datatype_get("size_t") != NULL);
}

static void test_dt_nonexistent(void)
{
	assert(retrace_datatype_get("nonexistent_dt") == NULL);
}

int main(void)
{
	/*
	 * Set up minimal retrace_real_impls so that the registry init
	 * functions can call strcmp/malloc/etc without crashing. These
	 * point to the real C library functions (no interposition in
	 * unit tests).
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

	printf("Initializing registries...\n");
	retrace_funcs_init();
	retrace_actions_init();
	retrace_datatypes_init();

	printf("Prototype registry:\n");
	TEST(proto_malloc);
	TEST(proto_printf_variadic);
	TEST(proto_pthread_mutex_lock);
	TEST(proto_strlen);
	TEST(proto_nonexistent);
	TEST(proto_cross_header);

	printf("Action registry:\n");
	TEST(action_log_params);
	TEST(action_call_real);
	TEST(action_modify_return_value);
	TEST(action_incomplete_io);
	TEST(action_fuzzing_seed);
	TEST(action_memory_fuzz);
	TEST(action_nonexistent);

	printf("Data type registry:\n");
	TEST(dt_int);
	TEST(dt_sz);
	TEST(dt_ptr);
	TEST(dt_size_t);
	TEST(dt_nonexistent);

	printf("\n%d run, %d passed, %d failed\n",
		tests_run, tests_pass, tests_fail);
	return tests_fail ? 1 : 0;
}
