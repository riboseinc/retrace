/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the audit PDF writer (TODO.complete/26 P2).
 *
 * The PDF is a compliance deliverable -- a malformed document
 * (bad header, broken xref, wrong object count) fails to open in
 * viewers and gets rejected by auditors' tooling. These tests
 * pin the structural contract of pdf_write_audit_report and the
 * escaping rules of pdf_escape_string.
 *
 * Covers:
 *   - pdf_escape_string: plain unchanged; ( ) and \ escaped;
 *     mixed strings; empty string yields ""
 *   - pdf_write_audit_report: PDF 1.4 header; %%EOF trailer;
 *     xref table + startxref present; Catalog/Pages/Font objects
 *   - page counts: 0/1/40 findings -> 3 pages; 41/45 -> 4/5
 *   - cover page carries policy name and trace path (escaped)
 *   - summary page carries the four severity counts
 *   - findings pages carry rule ids
 *   - special characters in identifiers survive the escaping
 */

#include "pdf_writer.h"

#include <stdio.h>
#include <stdlib.h>
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

/* Read an entire file into a malloc'd NUL-terminated buffer. */
static char *slurp(FILE *f)
{
	long sz;
	char *buf;

	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	rewind(f);
	buf = (char *)malloc((size_t)sz + 1);
	if (buf == NULL)
		return NULL;
	if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		return NULL;
	}
	buf[sz] = '\0';
	return buf;
}

/* Render a report with the given findings to a tmpfile; return
 * the PDF bytes (caller frees) and the writer's return value.
 */
static char *render(const char *policy, const char *trace,
		    int n_findings, int *rc_out)
{
	const char *sev0[] = {"critical", "high", "medium", "info"};
	const char *sev[64];
	const char *ids[64];
	const char *descs[64];
	FILE *f;
	char *pdf;
	int rc;
	int i;

	if (n_findings > 64)
		n_findings = 64;
	for (i = 0; i < n_findings; i++) {
		sev[i] = sev0[i % 4];
		ids[i] = "R-001";
		descs[i] = "finding description";
	}

	f = tmpfile();
	if (f == NULL)
		return NULL;
	rc = pdf_write_audit_report(f, policy, trace,
		n_findings > 0 ? sev : NULL,
		n_findings > 0 ? ids : NULL,
		n_findings > 0 ? descs : NULL,
		n_findings,
		1, 2, 3, 4);
	if (rc_out != NULL)
		*rc_out = rc;
	rewind(f);
	pdf = slurp(f);
	fclose(f);
	return pdf;
}

static int count_str(const char *hay, const char *needle)
{
	int n = 0;
	const char *p = hay;

	while ((p = strstr(p, needle)) != NULL) {
		n++;
		p += strlen(needle);
	}
	return n;
}

/* ----- pdf_escape_string ----- */

static void test_escape_plain_unchanged(void)
{
	char *e = pdf_escape_string("hello world");

	CHECK(e != NULL);
	CHECK(strcmp(e, "hello world") == 0);
	free(e);
}

static void test_escape_parens_and_backslash(void)
{
	char *e = pdf_escape_string("a(b)c\\d");

	CHECK(e != NULL);
	CHECK(strcmp(e, "a\\(b\\)c\\\\d") == 0);
	free(e);
}

static void test_escape_only_specials(void)
{
	char *e1 = pdf_escape_string("(");
	char *e2 = pdf_escape_string(")");
	char *e3 = pdf_escape_string("\\");

	CHECK(strcmp(e1, "\\(") == 0);
	CHECK(strcmp(e2, "\\)") == 0);
	CHECK(strcmp(e3, "\\\\") == 0);
	free(e1);
	free(e2);
	free(e3);
}

static void test_escape_empty_string(void)
{
	char *e = pdf_escape_string("");

	CHECK(e != NULL);
	CHECK(strcmp(e, "") == 0);
	free(e);
}

/* ----- document structure ----- */

static void test_header_and_trailer(void)
{
	char *pdf = render("pol", "/t.json", 1, NULL);
	size_t len;

	CHECK(pdf != NULL);
	len = strlen(pdf);
	CHECK(strncmp(pdf, "%PDF-1.4", 8) == 0);
	/* Trailer: %%EOF at the very end (allow trailing newline). */
	CHECK(strstr(pdf, "%%EOF") != NULL);
	CHECK(strstr(pdf + len - 16, "%%EOF") != NULL);
	free(pdf);
}

static void test_xref_and_core_objects(void)
{
	char *pdf = render("pol", "/t.json", 1, NULL);

	CHECK(pdf != NULL);
	CHECK(strstr(pdf, "xref") != NULL);
	CHECK(strstr(pdf, "startxref") != NULL);
	CHECK(strstr(pdf, "/Type /Catalog") != NULL);
	CHECK(strstr(pdf, "/Type /Pages") != NULL);
	CHECK(strstr(pdf, "/Type /Font") != NULL);
	CHECK(strstr(pdf, "/BaseFont /Helvetica") != NULL);
	/* The free entry of the xref table. */
	CHECK(strstr(pdf, "65535 f") != NULL);
	free(pdf);
}

/* ----- page counts ----- */

static void check_page_count(int n_findings, int want_pages,
			     const char *label)
{
	char *pdf = render("pol", "/t.json", n_findings, NULL);
	int pages;

	pages = pdf ? count_str(pdf, "/Type /Page ") : -1;
	printf("[%s: %d pages] ", label, pages);
	free(pdf);
	CHECK(pages == want_pages);
}

static void test_page_counts(void)
{
	/* cover + summary + >=1 findings page; 40 findings still
	 * fit on one findings page ((40+39)/40 == 1).
	 */
	check_page_count(0, 3, "0 findings");
	check_page_count(1, 3, "1 finding");
	check_page_count(40, 3, "40 findings");
}

static void test_page_counts_overflow_pages(void)
{
	/* 41 spills onto a second findings page. */
	check_page_count(41, 4, "41 findings");
}

/* ----- content ----- */

static void test_cover_carries_policy_and_trace(void)
{
	char *pdf = render("pci-dss", "/var/log/trace.json", 1, NULL);

	CHECK(pdf != NULL);
	CHECK(strstr(pdf, "pci-dss") != NULL);
	CHECK(strstr(pdf, "/var/log/trace.json") != NULL);
	CHECK(strstr(pdf, "retrace Compliance Audit Report") != NULL);
	free(pdf);
}

static void test_summary_carries_counts(void)
{
	char *pdf = render("pol", "/t.json", 1, NULL);

	CHECK(pdf != NULL);
	/* The writer pads the labels for column alignment. */
	CHECK(strstr(pdf, "Critical: 1") != NULL);
	CHECK(strstr(pdf, "High:     2") != NULL);
	CHECK(strstr(pdf, "Medium:   3") != NULL);
	CHECK(strstr(pdf, "Info:     4") != NULL);
	free(pdf);
}

static void test_findings_carry_rule_ids(void)
{
	char *pdf = render("pol", "/t.json", 5, NULL);
	int n;

	CHECK(pdf != NULL);
	n = count_str(pdf, "R-001");
	CHECK(n >= 5);
	free(pdf);
}

static void test_special_chars_survive_escaping(void)
{
	char *pdf;
	FILE *f;
	const char *sev[] = {"high"};
	const char *ids[] = {"R(1)\\x"};
	const char *descs[] = {"desc"};

	f = tmpfile();
	CHECK(f != NULL);
	pdf_write_audit_report(f, "pol(name)", "/t.json",
		sev, ids, descs, 1, 0, 1, 0, 0);
	rewind(f);
	pdf = slurp(f);
	fclose(f);

	CHECK(pdf != NULL);
	/* Escaped forms embedded in the content streams. */
	CHECK(strstr(pdf, "R\\(1\\)\\\\x") != NULL);
	CHECK(strstr(pdf, "pol\\(name\\)") != NULL);
	free(pdf);
}

static void test_writer_returns_zero(void)
{
	int rc = -99;

	render("pol", "/t.json", 3, &rc);
	CHECK(rc == 0);
}

int main(void)
{
	printf("-- pdf_escape_string --\n");
	TEST(escape_plain_unchanged);
	TEST(escape_parens_and_backslash);
	TEST(escape_only_specials);
	TEST(escape_empty_string);

	printf("-- document structure --\n");
	TEST(header_and_trailer);
	TEST(xref_and_core_objects);

	printf("-- page counts --\n");
	TEST(page_counts);
	TEST(page_counts_overflow_pages);

	printf("-- content --\n");
	TEST(cover_carries_policy_and_trace);
	TEST(summary_carries_counts);
	TEST(findings_carry_rule_ids);
	TEST(special_chars_survive_escaping);
	TEST(writer_returns_zero);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
