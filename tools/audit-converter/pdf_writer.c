/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Purpose-built PDF generator for compliance audit reports
 * (TODO.complete/26 P2).
 *
 * Produces a multi-page PDF with Helvetica text: cover page
 * (policy name, trace path), summary (severity counts), and
 * findings (one line per finding). Uses standard PDF 1.4
 * objects with byte-precise xref tracking.
 *
 * Not a general-purpose PDF library -- purpose-built for
 * retrace-audit's output format. Adding charts, images, or
 * custom fonts would require extending this file.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pdf_writer.h"

struct Buf {
	char *data;

	size_t len;

	size_t cap;
};

static void buf_init(struct Buf *b)
{
	b->cap = 4096;
	b->data = malloc(b->cap);
	b->data[0] = '\0';
	b->len = 0;
}

static void buf_printf(struct Buf *b, const char *fmt, ...)
{
	va_list ap;
	int n;

	while (1) {
		va_start(ap, fmt);
		n = vsnprintf(b->data + b->len, b->cap - b->len, fmt, ap);
		va_end(ap);
		if (n < 0)
			return;
		if ((size_t)n < b->cap - b->len) {
			b->len += n;
			return;
		}
		while (b->len + n + 1 > b->cap)
			b->cap *= 2;
		b->data = realloc(b->data, b->cap);
	}
}

static void buf_free(struct Buf *b)
{
	free(b->data);
}

char *pdf_escape_string(const char *s)
{
	size_t n = 0;
	const char *p;
	char *out;
	size_t i = 0;

	for (p = s; *p; p++) {
		if (*p == '(' || *p == ')' || *p == '\\')
			n += 2;
		else
			n++;
	}
	out = malloc(n + 1);
	if (out == NULL)
		return NULL;
	for (p = s; *p; p++) {
		if (*p == '(' || *p == ')' || *p == '\\') {
			out[i++] = '\\';
			out[i++] = *p;
		} else {
			out[i++] = *p;
		}
	}
	out[i] = '\0';
	return out;
}

/*
 * Build one PDF page's content stream from a list of text lines.
 * Each line is positioned at (72, y) with 14pt line height.
 * Returns a malloc'd string; caller frees.
 */
static char *build_page_content(const char *lines[], int n_lines)
{
	struct Buf b;

	buf_init(&b);
	buf_printf(&b, "BT\n/F1 11 Tf\n");
	for (int i = 0; i < n_lines; i++) {
		char *esc = pdf_escape_string(lines[i]);

		buf_printf(&b, "72 %d Td (%s) Tj 0 -14 Td\n",
			750 - i * 14, esc);
		free(esc);
	}
	buf_printf(&b, "ET\n");
	return b.data;
}

int pdf_write_audit_report(FILE *out, const char *policy_name,
			   const char *trace_path,
			   const char *const *severities,
			   const char *const *rule_ids,
			   const char *const *descriptions,
			   int n_findings,
			   int n_critical, int n_high,
			   int n_medium, int n_info)
{
	/* Object layout:
	 *   1: Catalog
	 *   2: Pages
	 *   3: Font (Helvetica)
	 *   4..4+2*N_pages-1: Page + Content pairs
	 */
	int n_pages = 2;  /* cover + summary */
	int n_finding_pages = (n_findings + 39) / 40;  /* 40 lines per page */

	n_pages += n_finding_pages > 0 ? n_finding_pages : 1;
	int total_objects = 3 + n_pages * 2;
	long *offsets = calloc(total_objects + 1, sizeof(long));
	struct Buf pdf;

	buf_init(&pdf);
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	char date_str[64];

	strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M", tm);

	/* Header */
	buf_printf(&pdf, "%%PDF-1.4\n");

	/* Object 1: Catalog */
	offsets[1] = (long)pdf.len;
	buf_printf(&pdf,
		"1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

	/* Object 2: Pages -- built later (need all page kids) */
	offsets[2] = (long)pdf.len;

	/* Object 3: Font */
	offsets[3] = (long)pdf.len;
	buf_printf(&pdf,
		"3 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n");

	/* Page objects start at 4, content streams at 5, alternating */
	int obj_id = 4;
	int page_ids[64];  /* max 64 pages */

	if (n_pages > 64)
		n_pages = 64;

	/* Page 1: Cover */
	{
		/* build_page_content escapes each line exactly once;
		 * pass the raw strings (pre-escaping here would
		 * double-escape and render literal backslashes).
		 */
		const char *lines[] = {
			"retrace Compliance Audit Report",
			"",
			"Policy:",
			"",
			policy_name,
			"",
			"Trace:",
			"",
			trace_path,
			"",
			"Date:",
			"",
			date_str,
		};
		char *content = build_page_content(lines, 13);

		page_ids[0] = obj_id;
		offsets[obj_id] = (long)pdf.len;
		buf_printf(&pdf,
			"%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents %d 0 R >>\nendobj\n",
			obj_id, obj_id + 1);
		obj_id++;
		offsets[obj_id] = (long)pdf.len;
		buf_printf(&pdf,
			"%d 0 obj\n<< /Length %zu >>\nstream\n%s\nendstream\nendobj\n",
			obj_id, strlen(content), content);
		obj_id++;
		free(content);
	}

	/* Page 2: Summary */
	{
		char summary_line1[128];
		char summary_line2[128];
		char summary_line3[128];
		char summary_line4[128];

		snprintf(summary_line1, sizeof(summary_line1),
			"Critical: %d", n_critical);
		snprintf(summary_line2, sizeof(summary_line2),
			"High:     %d", n_high);
		snprintf(summary_line3, sizeof(summary_line3),
			"Medium:   %d", n_medium);
		snprintf(summary_line4, sizeof(summary_line4),
			"Info:     %d", n_info);
		const char *lines[] = {
			"Audit Summary",
			"",
			summary_line1,
			summary_line2,
			summary_line3,
			summary_line4,
			"",
			"Total findings:",
			"",
		};
		char total_str[32];

		snprintf(total_str, sizeof(total_str), "%d", n_findings);
		{
			const char *all_lines[10];

			for (int i = 0; i < 9; i++)
				all_lines[i] = lines[i];
			all_lines[9] = total_str;
			{
				char *content = build_page_content(all_lines,
					10);

				page_ids[1] = obj_id;
				offsets[obj_id] = (long)pdf.len;
				buf_printf(&pdf,
					"%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents %d 0 R >>\nendobj\n",
					obj_id, obj_id + 1);
				obj_id++;
				offsets[obj_id] = (long)pdf.len;
				buf_printf(&pdf,
					"%d 0 obj\n<< /Length %zu >>\nstream\n%s\nendstream\nendobj\n",
					obj_id, strlen(content), content);
				obj_id++;
				free(content);
			}
		}
	}

	/* Pages 3+: Findings (40 per page) */
	{
		int finding_idx = 0;

		/* Object numbering: 3 core + 2 per page. After cover
		 * and summary the next findings page starts at obj_id
		 * and consumes exactly the remaining budget, so the
		 * guard is obj_id < total_objects (the historical
		 * "- 1" skipped the findings page whenever it fit
		 * exactly -- 1..40 findings produced a /Count of 3
		 * with only 2 page objects).
		 */
		while (finding_idx < n_findings && obj_id < total_objects) {
			const char *lines[42];
			int n_lines = 0;

			lines[n_lines++] = "Findings";
			lines[n_lines++] = "";
			for (int j = 0; j < 40 && finding_idx < n_findings;
			     j++) {
				static char finding_buf[40][200];

				snprintf(finding_buf[j], sizeof(finding_buf[0]),
					"[%s] %s: %.120s",
					severities[finding_idx],
					rule_ids[finding_idx],
					descriptions[finding_idx]);
				lines[n_lines++] = finding_buf[j];
				finding_idx++;
			}
			{
				int page_num = (finding_idx - 1) / 40 + 2;

				if (page_num < 64) {
					char *content = build_page_content(lines,
						n_lines);

					page_ids[page_num] = obj_id;
					offsets[obj_id] = (long)pdf.len;
					buf_printf(&pdf,
						"%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents %d 0 R >>\nendobj\n",
						obj_id, obj_id + 1);
					obj_id++;
					offsets[obj_id] = (long)pdf.len;
					buf_printf(&pdf,
						"%d 0 obj\n<< /Length %zu >>\nstream\n%s\nendstream\nendobj\n",
						obj_id, strlen(content), content);
					obj_id++;
					free(content);
				}
			}
		}
		if (n_findings == 0) {
			const char *lines[] = { "Findings", "",
				"No findings. The trace passed the policy." };
			char *content = build_page_content(lines, 3);

			page_ids[2] = obj_id;
			offsets[obj_id] = (long)pdf.len;
			buf_printf(&pdf,
				"%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents %d 0 R >>\nendobj\n",
				obj_id, obj_id + 1);
			obj_id++;
			offsets[obj_id] = (long)pdf.len;
			buf_printf(&pdf,
				"%d 0 obj\n<< /Length %zu >>\nstream\n%s\nendstream\nendobj\n",
				obj_id, strlen(content), content);
			obj_id++;
			free(content);
		}
	}

	/* Now fix up the Pages object (object 2) */
	{
		struct Buf pages_buf;

		buf_init(&pages_buf);
		buf_printf(&pages_buf,
			"2 0 obj\n<< /Type /Pages /Kids [");
		for (int i = 0; i < n_pages; i++) {
			if (page_ids[i] > 0)
				buf_printf(&pages_buf, "%d 0 R ", page_ids[i]);
		}
		buf_printf(&pages_buf, "] /Count %d >>\nendobj\n", n_pages);

		/* Rewrite object 2 at the correct offset */
		offsets[2] = (long)pdf.len;
		buf_printf(&pdf, "%s", pages_buf.data);
		buf_free(&pages_buf);
	}

	/* xref table */
	long xref_offset = (long)pdf.len;

	buf_printf(&pdf, "xref\n0 %d\n", obj_id);
	buf_printf(&pdf, "0000000000 65535 f\040\n");
	for (int i = 1; i < obj_id; i++)
		buf_printf(&pdf, "%010ld 00000 n\040\n", offsets[i]);

	buf_printf(&pdf,
		"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",
		obj_id, xref_offset);

	fwrite(pdf.data, 1, pdf.len, out);
	buf_free(&pdf);
	free(offsets);
	return 0;
}
