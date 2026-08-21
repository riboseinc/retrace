/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the kernel-truth converters (TODO.trace-profile/
 * 14): dtrace2retrace (macOS dtruss) and truss2retrace (FreeBSD).
 * Input lines are written to temp files; the converters parse
 * FILE* streams.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "convert.h"          /* dtrace */
#include "parson.h"

/* CHECK: assert() compiles to nothing under NDEBUG */
#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

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

/* truss_convert: the truss tool's convert.c is compiled into
 * this target directly (its convert.h would collide with the
 * dtrace one).
 */
extern int truss_convert(FILE *in, JSON_Array *out);

static FILE *temp_input(const char *text)
{
	FILE *f = tmpfile();

	if (f != NULL) {
		fputs(text, f);
		rewind(f);
	}
	return f;
}

static void test_dtrace_basic(void)
{
	FILE *in = temp_input(
		"  PID/TSYS  SYSCALL(args) \t = return\n"
		"  84546/0x30d7:  open_nocancel(\"/etc/hosts\\0\", 0x0, 0x0)\t\t = 3 0\n"
		"  84546/0x30d7:  stat64(\"/usr/local/lib\\0\", 0x7FFEE2, 0x0)\t\t = 0 0\n"
		"  84546/0x30d8:  mmap(0x0, 0x1000, 0x1)\t\t = 0 0\n"
		"  84547/0x30d9:  access(\"/tmp/nope\\0\", 0x0, 0x0)\t\t = -1 2\n");
	JSON_Value *root = json_value_init_array();
	JSON_Array *arr = json_value_get_array(root);
	JSON_Object *e;

	CHECK(in != NULL);
	CHECK(dtrace_convert(in, arr) == 3);
	fclose(in);

	/* open_nocancel -> open, \0 stripped, pid/tid parsed */
	e = json_array_get_object(arr, 0);
	CHECK(strcmp(json_object_get_string(
		json_object_get_object(e, "message"), "func"),
		"open") == 0);
	CHECK(strcmp(json_object_get_string(
		json_object_get_object(e, "message"), "path"),
		"/etc/hosts") == 0);
	CHECK((int)json_object_get_number(e, "pid") == 84546);
	CHECK((int)json_object_get_number(e, "tid") == 0x30d7);

	/* stat64 -> stat */
	e = json_array_get_object(arr, 1);
	CHECK(strcmp(json_object_get_string(
		json_object_get_object(e, "message"), "func"),
		"stat") == 0);

	/* noise (mmap header + non-file syscall) skipped; second
	 * pid parsed
	 */
	e = json_array_get_object(arr, 2);
	CHECK(strcmp(json_object_get_string(
		json_object_get_object(e, "message"), "path"),
		"/tmp/nope") == 0);
	CHECK((int)json_object_get_number(e, "pid") == 84547);

	json_value_free(root);
}

static void test_truss_basic(void)
{
	FILE *in = temp_input(
		"1234: openat(AT_FDCWD,\"/etc/master.passwd\",O_RDONLY,00) = 3 (0x0)\n"
		"1234: unlink(\"/tmp/gone\",0,0) = 0 (0x0)\n"
		"1234: sigprocmask(SIG_BLOCK,{ },NULL) = 0 (0x0)\n"
		"1235: stat(\"/a/b\",0x7fff) = 0 (0x0)\n");
	JSON_Value *root = json_value_init_array();
	JSON_Array *arr = json_value_get_array(root);
	JSON_Object *e;

	CHECK(in != NULL);
	CHECK(truss_convert(in, arr) == 3);
	fclose(in);

	e = json_array_get_object(arr, 0);
	CHECK(strcmp(json_object_get_string(
		json_object_get_object(e, "message"), "func"),
		"openat") == 0);
	CHECK(strcmp(json_object_get_string(
		json_object_get_object(e, "message"), "path"),
		"/etc/master.passwd") == 0);
	CHECK((int)json_object_get_number(e, "pid") == 1234);
	CHECK(strcmp(json_object_get_string(
		json_object_get_object(e, "message"), "detail"),
		"O_RDONLY") == 0);

	e = json_array_get_object(arr, 1);
	CHECK(strcmp(json_object_get_string(
		json_object_get_object(e, "message"), "func"),
		"unlink") == 0);

	e = json_array_get_object(arr, 2);
	CHECK((int)json_object_get_number(e, "pid") == 1235);

	json_value_free(root);
}

static void test_dtrace_noise_only(void)
{
	FILE *in = temp_input(
		"dtrace: script '/usr/bin/dtruss' matched 0 probes\n"
		"  123/0x1:  write(0x2, 0x7FFEE, 0x1)\t\t = 1 0\n");
	JSON_Value *root = json_value_init_array();
	JSON_Array *arr = json_value_get_array(root);

	CHECK(in != NULL);
	CHECK(dtrace_convert(in, arr) == 0);
	fclose(in);
	json_value_free(root);
}

int main(void)
{
	printf("kernel-truth converter tests:\n");
	TEST(dtrace_basic);
	TEST(truss_basic);
	TEST(dtrace_noise_only);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
