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
 * a forked child -- exactly like a real traced process.
 */

#include "parson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

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

static const char *g_log_path = "/tmp/retrace-test-fmt.json";

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
 * Fork; the child drives init -> two entries -> deinit with the
 * given FMT (NULL = unset, the default). Returns the log file
 * content owned by the caller.
 */
static char *run_logger_forked(const char *fmt)
{
	pid_t pid;
	int status = -1;

	remove(g_log_path);
	pid = fork();
	if (pid == 0) {
		JSON_Value *msg;

		setenv("RETRACE_LOGGER_DEF_ENA", "1", 1);
		setenv("RETRACE_LOGGER_DEF_STDOUT_ENA", "0", 1);
		setenv("RETRACE_LOGGER_DEF_FN", g_log_path, 1);
		setenv("RETRACE_LOGGER_RING", "0", 1);
		if (fmt != NULL)
			setenv("RETRACE_LOGGER_FMT", fmt, 1);
		else
			unsetenv("RETRACE_LOGGER_FMT");

		if (retrace_logger_init() != 0)
			_exit(3);

		msg = json_value_init_object();
		json_object_set_string(json_value_get_object(msg),
			"func", "open");
		retrace_logger_log_json(FUNCS, SEVERITY_INFO, msg);

		msg = json_value_init_object();
		json_object_set_string(json_value_get_object(msg),
			"func", "close");
		retrace_logger_log_json(FUNCS, SEVERITY_INFO, msg);

		retrace_logger_deinit();
		_exit(0);
	}
	if (waitpid(pid, &status, 0) != pid) {
		tests_fail++;
		return NULL;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		tests_fail++;
		return NULL;
	}
	return slurp(g_log_path);
}

static void
test_default_is_array_document(void)
{
	char *buf = run_logger_forked(NULL);

	CHECK(buf != NULL);
	CHECK(strncmp(buf, "[\n", 2) == 0);
	CHECK(strstr(buf, ",\n{") != NULL);
	CHECK(strstr(buf, "]\n") != NULL);
	free(buf);
}

static void
test_json_mode_is_array_document(void)
{
	char *buf = run_logger_forked("json");

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

	buf = run_logger_forked("jsonl");
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

	TEST(default_is_array_document);
	TEST(json_mode_is_array_document);
	TEST(jsonl_is_one_object_per_line);

	printf("%d run, %d pass, %d fail\n", tests_run, tests_pass,
		tests_fail);
	return (tests_fail == 0) ? 0 : 1;
}
