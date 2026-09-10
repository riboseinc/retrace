/*
 * Copyright (c) 2017, [Ribose Inc](https://ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * The standalone chain primitive (TODO.impl/07): one walk,
 * three facts -- verified?, head-at-stop, line count. The
 * signer seals the full-walk head; the verifier re-derives the
 * head at the seal point; both ride THIS arithmetic.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "journal.h"

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

static void write_journal(const char *path, int events)
{
	struct retraced_journal j;
	int i;

	retraced_journal_open(&j, path);
	for (i = 0; i < events; i++)
		retraced_journal_event(&j, 1000 + i, "t", i,
			"{\"name\":\"e\"}");
	retraced_journal_close(&j);
}

static void test_chain_walk_reports_head_and_lines(void)
{
	uint64_t head = 0, head_all = 0;
	size_t lines = 0, broken = 99;

	remove("/tmp/jchain1.jsonl");
	write_journal("/tmp/jchain1.jsonl", 3);
	/* close adds one record: 4 lines total */
	CHECK(retraced_journal_chain_file(
		"/tmp/jchain1.jsonl", 0, &head_all, &lines,
		&broken) == 0);
	CHECK(lines == 4);
	CHECK(broken == 0);
	CHECK(head_all != 0);
	/* stop before the close marker: 3 events */
	CHECK(retraced_journal_chain_file(
		"/tmp/jchain1.jsonl", 3, &head, NULL, &broken) == 0);
	CHECK(head != head_all);
	CHECK(head != 0);
	/* stop past the end is the full walk */
	CHECK(retraced_journal_chain_file(
		"/tmp/jchain1.jsonl", 99, &head, NULL, &broken) == 0);
	CHECK(head == head_all);
	remove("/tmp/jchain1.jsonl");
}

static void test_chain_names_the_broken_line(void)
{
	char all[8192];
	size_t lines = 0, broken = 0;
	FILE *f;

	remove("/tmp/jchain2.jsonl");
	write_journal("/tmp/jchain2.jsonl", 3);
	f = fopen("/tmp/jchain2.jsonl", "r");
	CHECK(f != NULL);
	(void)fread(all, 1, sizeof(all) - 1, f);
	fclose(f);
	/* tamper line 2's payload (not its prev field) */
	{
		char *p = strstr(all, "\"name\":\"e\"");

		if (p != NULL)
			*p = 'x';
	}
	f = fopen("/tmp/jchain2.jsonl", "w");
	fputs(all, f);
	fclose(f);
	CHECK(retraced_journal_chain_file(
		"/tmp/jchain2.jsonl", 0, NULL, &lines,
		&broken) != 0);
	/* the tampered line is the FIRST event (line 1): its link
	 * changes, so line 2's stored prev no longer matches
	 */
	CHECK(broken == 2);
	remove("/tmp/jchain2.jsonl");
}

static void test_missing_file_is_an_error(void)
{
	size_t broken = 0;

	CHECK(retraced_journal_chain_file(
		"/tmp/jchain-nonexistent.jsonl", 0, NULL, NULL,
		&broken) != 0);
}

int main(void)
{
	printf("journal chain primitive tests:\n");
	TEST(chain_walk_reports_head_and_lines);
	TEST(chain_names_the_broken_line);
	TEST(missing_file_is_an_error);
	printf("%d tests: %d fail\n", tests_run, tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
