/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Unit: journal rotation + retention (TODO.impl/10). Proves the
 * lifecycle in isolation from the daemon:
 *
 *  - a byte cap produces a numbered segment series; each
 *    segment's chain verifies on its own
 *  - each new segment's genesis carries the predecessor's
 *    final head -- one chain, split across files
 *  - replay over the whole series rebuilds state and the
 *    cross-segment links verify (fail-closed on tampering)
 *  - retention under a byte budget prunes OLDEST segments only,
 *    the live segment survives, and every prune is a chained
 *    record in the live segment (auditable gaps)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "journal.h"

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

static void ev(struct retraced_journal *j, const char *name)
{
	char payload[160];

	snprintf(payload, sizeof(payload),
		"{\"name\":\"%s\"}", name);
	retraced_journal_event(j, (long) time(NULL), "daemon", 0,
		payload);
}

static int count_lines(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[2048];
	int n = 0;

	if (f == NULL)
		return -1;
	while (fgets(line, sizeof(line), f) != NULL)
		n++;
	fclose(f);
	return n;
}

static int file_has(const char *path, const char *needle)
{
	FILE *f = fopen(path, "r");
	char line[2048];

	if (f == NULL)
		return 0;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, needle) != NULL) {
			fclose(f);
			return 1;
		}
	}
	fclose(f);
	return 0;
}

static void test_rotation_segments_and_chains(void)
{
	struct retraced_journal j;
	char base[256];
	char p[600];
	uint64_t head;
	size_t lines, broken;
	int seq;

	snprintf(base, sizeof(base), "/tmp/jrot-%ld.jsonl",
		(long) getpid());
	snprintf(p, sizeof(p), "%s.0000", base);
	remove(p);

	retraced_journal_open(&j, base);
	retraced_journal_set_rotation(&j, 600, 0, 0);

	/* enough durable events to cross the cap several times */
	{
		int i;

		for (i = 0; i < 40; i++)
			ev(&j, i % 2 ?
			    "retrace.policy.pushed" :
			    "retrace.session.minted");
	}
	retraced_journal_close(&j);

	/* a SERIES exists (more than one segment) */
	{
		int n = 0;

		for (seq = 0; seq < 100; seq++) {
			retraced_journal_segment_path(base, seq, p,
				sizeof(p));
			if (!retraced_journal_segment_exists(p, NULL))
				break;
			n++;
		}
		CHECK(n >= 2);
		printf("[%d segs] ", n);
	}

	/* every segment verifies, each from its predecessor's
	 * final head (the chain continues across files)
	 */
	{
		uint64_t carry = 0;

		for (seq = 0; seq < 100; seq++) {
			retraced_journal_segment_path(base, seq, p,
				sizeof(p));
			if (!retraced_journal_segment_exists(p, NULL))
				break;
			CHECK(retraced_journal_chain_file_from(p, carry,
				  0, &head, &lines, &broken) == 0);
			carry = head;
		}
	}

	/* cross-segment link: every CLOSED segment carries the
	 * marker; the FINAL live segment legitimately does not
	 * (it is still open -- the daemon's journal.closed or an
	 * unclean tail lives there instead)
	 */
	{
		int n_segs = 0;

		for (seq = 0; seq < 100; seq++) {
			retraced_journal_segment_path(base, seq, p,
				sizeof(p));
			if (!retraced_journal_segment_exists(p, NULL))
				break;
			n_segs++;
		}
		for (seq = 0; seq < n_segs - 1; seq++) {
			retraced_journal_segment_path(base, seq, p,
				sizeof(p));
			CHECK(file_has(p,
				"retrace.journal.segment_closed"));
		}
		CHECK(file_has(p, "retrace.journal.segment_opened") ||
		      n_segs == 1);
	}

	/* replay over the series: verifies every link across
	 * segments and leaves the head at the live tail
	 */
	{
		struct retraced_journal j2;
		struct retraced_registry r;

		retraced_registry_init(&r);
		retraced_journal_open(&j2, base);
		retraced_journal_set_rotation(&j2, 600, 0, 0);
		CHECK(retraced_journal_replay(&j2, &r) == 0);
		CHECK(j2.prev_hash != 0);
	}

	/* cleanup */
	for (seq = 0; seq < 100; seq++) {
		retraced_journal_segment_path(base, seq, p, sizeof(p));
		if (!retraced_journal_segment_exists(p, NULL))
			break;
		remove(p);
	}
}

static void test_retention_prunes_oldest(void)
{
	struct retraced_journal j;
	char base[256];
	char p[600];
	int seq;

	snprintf(base, sizeof(base), "/tmp/jret-%ld.jsonl",
		(long) getpid());

	/* write enough to make ~4 segments under a tiny budget */
	retraced_journal_open(&j, base);
	retraced_journal_set_rotation(&j, 400, 0, 1200);
	{
		int i;

		for (i = 0; i < 60; i++)
			ev(&j, "retrace.policy.pushed");
	}
	retraced_journal_close(&j);

	/* rotation ran to a high sequence number (probe from the
	 * top: retention removed the low end) and the OLDEST
	 * segments are gone (pruned under the budget)
	 */
	{
		int hi = -1;

		for (seq = 9999; seq >= 0 && hi < 0; seq--) {
			retraced_journal_segment_path(base, seq, p,
				sizeof(p));
			if (retraced_journal_segment_exists(p, NULL))
				hi = seq;
		}
		CHECK(hi >= 2);
		retraced_journal_segment_path(base, 0, p, sizeof(p));
		CHECK(!retraced_journal_segment_exists(p, NULL));
	}

	/* the live segment carries the prune records (auditable) */
	{
		int found = 0;

		for (seq = 0; seq < 100; seq++) {
			retraced_journal_segment_path(base, seq, p,
				sizeof(p));
			if (!retraced_journal_segment_exists(p, NULL))
				continue;
			if (file_has(p, "retrace.journal.segment_pruned"))
				found = 1;
		}
		CHECK(found);
	}

	/* replay over what remains still verifies (the prunes are
	 * chained records; the surviving series is intact)
	 */
	{
		struct retraced_journal j2;
		struct retraced_registry r;

		retraced_registry_init(&r);
		retraced_journal_open(&j2, base);
		retraced_journal_set_rotation(&j2, 400, 0, 0);
		CHECK(retraced_journal_replay(&j2, &r) == 0);
	}

	for (seq = 0; seq < 100; seq++) {
		retraced_journal_segment_path(base, seq, p, sizeof(p));
		if (!retraced_journal_segment_exists(p, NULL))
			break;
		remove(p);
	}
}

static void test_tampered_middle_segment_fails_closed(void)
{
	struct retraced_journal j;
	char base[256];
	char p[600];
	FILE *f;

	snprintf(base, sizeof(base), "/tmp/jtam-%ld.jsonl",
		(long) getpid());

	retraced_journal_open(&j, base);
	retraced_journal_set_rotation(&j, 400, 0, 0);
	{
		int i;

		for (i = 0; i < 40; i++)
			ev(&j, "retrace.policy.pushed");
	}
	retraced_journal_close(&j);

	/* tamper with the FIRST surviving segment's middle */
	retraced_journal_segment_path(base, 0, p, sizeof(p));
	if (retraced_journal_segment_exists(p, NULL)) {
		f = fopen(p, "r+");
		if (f != NULL) {
			char line[2048];

			/* rewrite line 2's payload (chain breaks
			 * at line 3)
			 */
			fgets(line, sizeof(line), f);
			fgets(line, sizeof(line), f);
			fseek(f, -((long)strlen(line)), SEEK_CUR);
			fputs("{\"ev\":{\"name\":\"x\"}}\n", f);
			fclose(f);
		}

		{
			struct retraced_journal j2;
			struct retraced_registry r;

			retraced_registry_init(&r);
			retraced_journal_open(&j2, base);
			retraced_journal_set_rotation(&j2, 400, 0, 0);
			CHECK(retraced_journal_replay(&j2, &r) == -1);
		}
	} else {
		printf("[no seg 0; tampered live] ");
		retraced_journal_segment_path(base, 1, p, sizeof(p));
		f = fopen(p, "r+");
		if (f != NULL) {
			char line[2048];

			fgets(line, sizeof(line), f);
			fseek(f, -((long)strlen(line)), SEEK_CUR);
			fputs("{\"ev\":{\"name\":\"x\"}}\n", f);
			fclose(f);
		}
		{
			struct retraced_journal j2;
			struct retraced_registry r;

			retraced_registry_init(&r);
			retraced_journal_open(&j2, base);
			retraced_journal_set_rotation(&j2, 400, 0, 0);
			CHECK(retraced_journal_replay(&j2, &r) == -1);
		}
	}

	{
		int seq;

		for (seq = 0; seq < 100; seq++) {
			retraced_journal_segment_path(base, seq, p,
				sizeof(p));
			if (!retraced_journal_segment_exists(p, NULL))
				break;
			remove(p);
		}
	}
}

int
main(void)
{
	printf("journal lifecycle (TODO.impl/10)\n");

	TEST(rotation_segments_and_chains);
	TEST(retention_prunes_oldest);
	TEST(tampered_middle_segment_fails_closed);

	printf("%d run, %d pass, %d fail\n",
		tests_run, tests_pass, tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
