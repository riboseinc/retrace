/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the procmon CSV scanner (TODO.next-level/06).
 *
 * Procmon's CSV export quotes fields that contain commas, uses
 * CRLF record ends, may carry a UTF-8 BOM, and the Detail column
 * can embed CRLF and doubled quotes. Every one of those shapes
 * must yield exactly the fields the writer intended.
 */

#include "csv.h"

#include <stdio.h>
#include <string.h>

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

struct Capture {
	struct PmCsvRow rows[8];
	size_t		count;
	size_t		skipped;
};

/*
 * The scanner reuses (and later frees) the row's storage; copy
 * the storage and rebase the field pointers into the copy.
 */
static void
capture(const struct PmCsvRow *row, void *ctx)
{
	struct Capture *cap = (struct Capture *) ctx;
	size_t		i;

	if (cap->count < 8) {
		memcpy(&cap->rows[cap->count], row, sizeof(*row));
		for (i = 0; i < row->count; i++) {
			cap->rows[cap->count].fields[i] =
			  cap->rows[cap->count].storage + (row->fields[i] - row->storage);
		}
	}
	cap->count++;
}

static size_t
run(const char *text, struct Capture *cap)
{
	memset(cap, 0, sizeof(*cap));
	return pmcsv_scan(text, strlen(text), capture, cap, &cap->skipped);
}

static void
test_plain_fields(void)
{
	struct Capture cap;

	CHECK(run("a,b,c", &cap) == 1);
	CHECK(cap.rows[0].count == 3);
	CHECK(strcmp(cap.rows[0].fields[0], "a") == 0);
	CHECK(strcmp(cap.rows[0].fields[1], "b") == 0);
	CHECK(strcmp(cap.rows[0].fields[2], "c") == 0);
}

static void
test_procmon_header_row(void)
{
	struct Capture cap;

	CHECK(run("\"Time of Day\",\"Process Name\",\"PID\",\"Operation\","
		  "\"Path\",\"Result\",\"Detail\"",
		  &cap) == 1);
	CHECK(cap.rows[0].count == 7);
	CHECK(strcmp(cap.rows[0].fields[0], "Time of Day") == 0);
	CHECK(strcmp(cap.rows[0].fields[3], "Operation") == 0);
	CHECK(strcmp(cap.rows[0].fields[6], "Detail") == 0);
}

static void
test_quoted_comma_and_spaces(void)
{
	struct Capture cap;

	CHECK(run("\"C:\\Program Files\\Vendor\\app.exe\",CreateFile", &cap) == 1);
	CHECK(cap.rows[0].count == 2);
	CHECK(strcmp(cap.rows[0].fields[0], "C:\\Program Files\\Vendor\\app.exe") == 0);
	CHECK(strcmp(cap.rows[0].fields[1], "CreateFile") == 0);
}

static void
test_doubled_quote(void)
{
	struct Capture cap;

	CHECK(run("\"say \"\"hi\"\", ok\",x", &cap) == 1);
	CHECK(cap.rows[0].count == 2);
	CHECK(strcmp(cap.rows[0].fields[0], "say \"hi\", ok") == 0);
}

static void
test_embedded_crlf_in_quoted_field(void)
{
	struct Capture cap;

	/* Detail carrying a CRLF: one record, not three. */
	CHECK(run("\"CreateFile\",\"line1\r\nline2\",SUCCESS\r\n"
		  "\"next\",\"row\",OK\r\n",
		  &cap) == 2);
	CHECK(cap.rows[0].count == 3);
	CHECK(strcmp(cap.rows[0].fields[1], "line1\r\nline2") == 0);
	CHECK(strcmp(cap.rows[1].fields[0], "next") == 0);
}

static void
test_bom_and_crlf_records(void)
{
	struct Capture cap;
	const char *doc = "\xEF\xBB\xBF\"a\",1\r\n\"b\",2\r\n";
	size_t	       n = 0;

	memset(&cap, 0, sizeof(cap));
	n = pmcsv_scan(doc, strlen(doc), capture, &cap, &cap.skipped);
	CHECK(n == 2);
	CHECK(cap.rows[0].fields[0][0] == 'a');
	CHECK(cap.rows[1].fields[0][0] == 'b');
}

static void
test_blank_lines_skipped(void)
{
	struct Capture cap;

	CHECK(run("a,b\r\n\r\nc,d\r\n", &cap) == 2);
	CHECK(cap.rows[0].count == 2);
	CHECK(cap.rows[1].count == 2);
}

static void
test_trailing_comma_and_no_newline(void)
{
	struct Capture cap;

	CHECK(run("a,b,", &cap) == 1);
	CHECK(cap.rows[0].count == 3);
	CHECK(cap.rows[0].fields[2][0] == '\0');
}

static void
test_eof_inside_quote_skipped(void)
{
	struct Capture cap;

	CHECK(run("\"good\",1\r\n\"unterminated, oops", &cap) == 1);
	CHECK(cap.skipped == 1);
	CHECK(strcmp(cap.rows[0].fields[0], "good") == 0);
}

static void
test_long_field_truncated(void)
{
	struct Capture cap;
	char	       big[PMCSV_FIELD_MAX + 512];
	size_t	       n = 0;

	memset(big, 'x', sizeof(big) - 1);
	big[sizeof(big) - 1] = '\0';
	memset(&cap, 0, sizeof(cap));
	n = pmcsv_scan(big, sizeof(big) - 1, capture, &cap, &cap.skipped);
	CHECK(n == 1);
	CHECK(strlen(cap.rows[0].fields[0]) == PMCSV_FIELD_MAX - 1);
}

static void
test_null_safety(void)
{
	struct Capture cap;

	CHECK(pmcsv_scan(NULL, 5, capture, &cap, NULL) == 0);
	CHECK(pmcsv_scan("a", 1, NULL, &cap, NULL) == 0);
	CHECK(pmcsv_scan("", 0, capture, &cap, NULL) == 0);
}

int
main(void)
{
	printf("procmon csv tests\n");
	TEST(plain_fields);
	TEST(procmon_header_row);
	TEST(quoted_comma_and_spaces);
	TEST(doubled_quote);
	TEST(embedded_crlf_in_quoted_field);
	TEST(bom_and_crlf_records);
	TEST(blank_lines_skipped);
	TEST(trailing_comma_and_no_newline);
	TEST(eof_inside_quote_skipped);
	TEST(long_field_truncated);
	TEST(null_safety);
	printf("%d run, %d pass, %d fail\n", tests_run, tests_pass, tests_fail);
	return (tests_fail == 0) ? 0 : 1;
}
