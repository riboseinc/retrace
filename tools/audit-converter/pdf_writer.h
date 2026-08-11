/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_AUDIT_PDF_WRITER_H_
#define RETRACE_AUDIT_PDF_WRITER_H_

#include <stdio.h>

/*
 * Purpose-built PDF generator for compliance audit reports
 * (TODO.complete/26 P2). Produces a multi-page PDF with
 * Helvetica text: cover, summary, findings.
 *
 * Decoupled from the Finding struct -- takes arrays of strings.
 */

int pdf_write_audit_report(FILE *out, const char *policy_name,
			   const char *trace_path,
			   const char *const *severities,
			   const char *const *rule_ids,
			   const char *const *descriptions,
			   int n_findings,
			   int n_critical, int n_high,
			   int n_medium, int n_info);

char *pdf_escape_string(const char *s);

#endif /* RETRACE_AUDIT_PDF_WRITER_H_ */
