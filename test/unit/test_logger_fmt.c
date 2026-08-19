/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Logger output-format tests (TODO.windows/07).
 *
 * RETRACE_LOGGER_FMT selects between the default single JSON
 * array document (leading-comma emission, brackets opened at
 * init and closed at deinit) and JSONL (one compact object per
 * line, no brackets). The logger keeps process-global state
 * (first-entry flag, ring readiness), so each scenario runs in
 * a fresh process driven by RETRACE_FMT_UNDER_TEST -- exactly
 * like a real traced process, identically on POSIX and Windows.
 */

#include "parson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "parson.h"

#ifdef _WIN32
static int rc_setenv_win(const char *n, const char *v,
	int ow);
static int rc_unsetenv_win(const char *n);
/* MinGW/MSVC have no setenv/unsetenv; the CRT spelling is
 * _putenv with "NAME=" clearing.
 */
static int rc_setenv_win(const char *name, const char *val,
	int overwrite)
{
	char buf[512];

	(void)overwrite;
	snprintf(buf, sizeof(buf), "%s=%s", name, val);
	return _putenv(buf);
}

static int rc_unsetenv_win(const char *name)
{
	char buf[512];

	snprintf(buf, sizeof(buf), "%s=", name);
	return _putenv(buf);
}
#else
#include <unistd.h>
#define rc_setenv_win(n, v, o) setenv(n, v, o)
#define rc_unsetenv_win(n) unsetenv(n)
#endif
#include "logger.h"
#include "real_impls.h"

static int tests_run;
static int tests_pass;
static int tests_fail;

#define TEST(name)                               \
	do {                                     \
		tests_run++;                     \
		printf("  TEST %s ... ", #name); \
		test_##name();                   \
		tests_pass++;                    \
		printf("OK\n");                  \
	} while (0)

#define CHECK(cond)                                                             \
	do {                                                                    \
		if (!(cond)) {                                                  \
			printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
			tests_fail++;                                           \
			return;                                                 \
		}                                                               \
	} while (0)

static const char *g_log_path = "retrace-test-fmt.json";

static char *slurp(const char *path)
{
	FILE *f = fopen(path, "rb");
	long sz;
	char *buf;

	if (f == NULL)
		return NULL;
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = malloc((size_t)sz + 1);
	if (buf == NULL || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		fclose(f);
		return NULL;
	}
	fclose(f);
	buf[sz] = '\0';
	return buf;
}

/*
 * Drive init -> two entries -> deinit with the given FMT (NULL =
 * unset, the default). The logger keeps process-global state
 * (first-entry flag, ring readiness), so each ctest invocation
 * runs ONE scenario in a fresh process; the scenario is chosen
 * by RETRACE_FMT_UNDER_TEST (unset = the default format). This
 * is fork-free and runs identically on POSIX and Windows.
 */
static char *run_logger(const char *fmt)
{
	JSON_Value *msg;

	remove(g_log_path);
	rc_setenv_win("RETRACE_LOGGER_DEF_ENA", "1", 1);
	rc_setenv_win("RETRACE_LOGGER_DEF_STDOUT_ENA", "0", 1);
	rc_setenv_win("RETRACE_LOGGER_DEF_FN", g_log_path, 1);
	rc_setenv_win("RETRACE_LOGGER_RING", "0", 1);
	if (fmt != NULL)
		rc_setenv_win("RETRACE_LOGGER_FMT", fmt, 1);
	else
		rc_unsetenv_win("RETRACE_LOGGER_FMT");

	if (retrace_logger_init() != 0) {
		tests_fail++;
		return NULL;
	}

	msg = json_value_init_object();
	json_object_set_string(json_value_get_object(msg),
		"func", "open");
	retrace_logger_log_json(FUNCS, SEVERITY_INFO, msg);

	msg = json_value_init_object();
	json_object_set_string(json_value_get_object(msg),
		"func", "close");
	retrace_logger_log_json(FUNCS, SEVERITY_INFO, msg);

	retrace_logger_deinit();
	return slurp(g_log_path);
}

/* One scenario per process: chosen by the test harness env. */
static int scenario_is(const char *want)
{
	const char *v = getenv("RETRACE_FMT_UNDER_TEST");

	if (want == NULL)
		return v == NULL;
	return v != NULL && strcmp(v, want) == 0;
}

/*
 * Regression (v2.11.1): entries pushed to the ring must survive
 * an immediate exit -- deinit's final drain had been dead code
 * after the late-call gate cleared the ring-ready flag before
 * the guards that used it.
 */
static void
test_ring_drains_on_immediate_exit(void)
{
	char *buf;
	char *copy;
	char *line;
	int lines = 0;

	/* Ring left ENABLED (no RETRACE_LOGGER_RING env). */
	remove(g_log_path);
	rc_setenv_win("RETRACE_LOGGER_DEF_ENA", "1", 1);
	rc_setenv_win("RETRACE_LOGGER_DEF_STDOUT_ENA", "0", 1);
	rc_setenv_win("RETRACE_LOGGER_DEF_FN", g_log_path, 1);
	rc_unsetenv_win("RETRACE_LOGGER_RING");

	CHECK(retrace_logger_init() == 0);

	{
		JSON_Value *msg = json_value_init_object();

		json_object_set_string(json_value_get_object(msg),
			"func", "open");
		retrace_logger_log_json(FUNCS, SEVERITY_INFO, msg);
	}

	retrace_logger_deinit();

	buf = slurp(g_log_path);
	CHECK(buf != NULL);
	copy = strdup(buf);
	line = strtok(copy, "\n");
	while (line != NULL) {
		if (line[0] == '{')
			lines++;
		line = strtok(NULL, "\n");
	}
	free(copy);
	CHECK(lines >= 1);
	free(buf);
}

static void
test_default_is_array_document(void)
{
	char *buf = run_logger(NULL);

	CHECK(buf != NULL);
	CHECK(strncmp(buf, "[\n", 2) == 0);
	CHECK(strstr(buf, ",\n{") != NULL);
	CHECK(strstr(buf, "]\n") != NULL);
	free(buf);
}

static void
test_json_mode_is_array_document(void)
{
	char *buf = run_logger("json");

	CHECK(buf != NULL);
	CHECK(strncmp(buf, "[\n", 2) == 0);
	CHECK(strstr(buf, ",\n{") != NULL);
	CHECK(strstr(buf, "]\n") != NULL);
	free(buf);
}

static void
test_jsonl_is_one_object_per_line(void)
{
	char *buf;
	char *copy;
	char *line;
	int lines = 0;

	buf = run_logger("jsonl");
	CHECK(buf != NULL);
	CHECK(buf[0] == '{');

	copy = strdup(buf);
	line = strtok(copy, "\n");
	while (line != NULL) {
		size_t n = strlen(line);

		if (n > 0) {
			CHECK(line[0] == '{');
			CHECK(line[n - 1] == '}');
			/* Complete standalone JSON on every line. */
			CHECK(json_parse_string(line) != NULL);
			lines++;
		}
		line = strtok(NULL, "\n");
	}
	free(copy);
	CHECK(lines == 2);
	CHECK(strstr(buf, ",\n") == NULL);
	CHECK(strstr(buf, "]\n") == NULL);
	free(buf);
}

int
main(void)
{
	printf("logger format tests\n");

	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.fopen = fopen;
	retrace_real_impls.fclose = fclose;
	retrace_real_impls.fprintf = fprintf;
	retrace_real_impls.printf = printf;
	retrace_real_impls.fflush = fflush;
	retrace_real_impls.time = time;
	retrace_real_impls.getenv = getenv;
	retrace_real_impls.atoi = atoi;

	/*
	 * MSVC has no constructors: drive the real-impls init
	 * explicitly (idempotent on POSIX, whose constructor
	 * already ran it).
	 */
	retrace_real_impls_init();

	if (scenario_is("json"))
		TEST(json_mode_is_array_document);
	else if (scenario_is("jsonl"))
		TEST(jsonl_is_one_object_per_line);
	else if (scenario_is("ring"))
		TEST(ring_drains_on_immediate_exit);
	else
		TEST(default_is_array_document);

	printf("%d run, %d pass, %d fail\n", tests_run, tests_pass,
		tests_fail);
	return (tests_fail == 0) ? 0 : 1;
}
