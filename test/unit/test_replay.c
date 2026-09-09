/*
 * Copyright (c) 2017, [Ribose Inc](https://ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * The replay slice's module contract (TODO.impl/03): recording
 * persists the seed and every synthesized outcome; replay
 * forces the recorded seed back. (The drift verdict is the
 * E2E's to assert through the report log.)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "replay.h"

static int tests_run;
static int tests_fail;

#define TEST(name) do { \
	tests_run++; \
	printf("  TEST %s ... ", #name); \
	test_##name(); \
	tests_fail += g_failed; \
	printf("%s\n", g_failed ? "FAIL" : "OK"); \
	g_failed = 0; \
} while (0)

static int g_failed;

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("\n    CHECK failed [%s:%d] %s\n", __FILE__, \
			__LINE__, #cond); \
		g_failed = 1; \
		return; \
	} \
} while (0)

static void read_all(const char *path, char *out, size_t cap)
{
	FILE *f = fopen(path, "r");

	out[0] = '\0';
	if (f == NULL)
		return;
	(void)fread(out, 1, cap - 1, f);
	out[cap - 1] = '\0';
	fclose(f);
}

static void test_record_persists_seed_and_outcomes(void)
{
	const char *rec = "/tmp/replay_unit.rec";
	char buf[4096];

	remove(rec);
	setenv("RETRACE_REPLAY_OUT", rec, 1);
	setenv("RETRACE_REPLAY_IN", "", 1);
	unsetenv("RETRACE_REPLAY_IN");
	CHECK(retrace_replay_mode() == 1);
	CHECK(retrace_replay_seed(1234) == 1234);
	retrace_replay_note("malloc", "memfuzz", "fail");
	retrace_replay_note("open", "fuzz_str", "AAAA");
	retrace_replay_report();
	read_all(rec, buf, sizeof(buf));
	CHECK(strstr(buf, "seed 1234\n") == buf);
	CHECK(strstr(buf, "\nmalloc memfuzz fail\n") != NULL);
	CHECK(strstr(buf, "\nopen fuzz_str AAAA\n") != NULL);
	unsetenv("RETRACE_REPLAY_OUT");
	remove(rec);
}

static void test_replay_forces_the_recorded_seed(void)
{
	const char *rec = "/tmp/replay_unit2.rec";
	FILE *f;

	/* the record arrives from a PRIOR process: write it
	 * directly (mode is write-once -- no in-process switching)
	 */
	f = fopen(rec, "w");
	CHECK(f != NULL);
	fprintf(f, "seed 777\nmalloc memfuzz fail\n");
	fclose(f);

	setenv("RETRACE_REPLAY_IN", rec, 1);
	CHECK(retrace_replay_mode() == 2);
	/* the time fallback would never say 777 again on purpose */
	CHECK(retrace_replay_seed(999) == 777);
	retrace_replay_note("malloc", "memfuzz", "fail");
	retrace_replay_report();
	unsetenv("RETRACE_REPLAY_IN");
	remove(rec);
}

/*
 * The module's mode is write-once per process (records are
 * streams, not switchable state) -- so the harness runs ONE
 * phase per invocation and ctest registers both.
 */
int main(int argc, char **argv)
{
	const char *phase = argc > 1 ? argv[1] : "record";

	if (strcmp(phase, "record") == 0) {
		tests_run++;
		printf("  TEST record_persists_seed_and_outcomes ... ");
		test_record_persists_seed_and_outcomes();
		tests_fail += g_failed;
		printf("%s\n", g_failed ? "FAIL" : "OK");
		g_failed = 0;
	} else if (strcmp(phase, "replay") == 0) {
		tests_run++;
		printf("  TEST replay_forces_the_recorded_seed ... ");
		test_replay_forces_the_recorded_seed();
		tests_fail += g_failed;
		printf("%s\n", g_failed ? "FAIL" : "OK");
		g_failed = 0;
	} else if (strcmp(phase, "off") == 0) {
		tests_run++;
		printf("  TEST off_by_default ... ");
		unsetenv("RETRACE_REPLAY_OUT");
		unsetenv("RETRACE_REPLAY_IN");
		if (retrace_replay_mode() != 0) {
			printf("\n    mode armed with no env\n");
			tests_fail++;
			g_failed = 1;
		} else {
			retrace_replay_note("x", "y", "z");
			retrace_replay_report();
		}
		printf("%s\n", g_failed ? "FAIL" : "OK");
		g_failed = 0;
	} else {
		fprintf(stderr, "usage: test_replay [record|replay|off]\n");
		return 2;
	}
	printf("%d tests: %d fail\n", tests_run, tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
