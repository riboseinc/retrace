/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-audit: compliance audit report generator
 * (TODO.complete/26).
 *
 * Reads a retrace JSON log, applies each policy rule's predicate
 * against each entry, and emits findings as JSON.
 *
 * Usage:
 *   retrace run --log /tmp/trace.json -- ./your-binary
 *   retrace-audit --policy share/policies/baseline.json \
 *                 --trace /tmp/trace.json [--format default|sarif]
 *
 * Output formats:
 *   default  -- human-readable JSON with policy, summary, findings
 *               array. Each finding includes the rule_id, severity,
 *               description, entry_index, and the original log
 *               entry for evidence.
 *   sarif    -- SARIF 2.1.0 (industry-standard static-analysis
 *               format). Consumed natively by GitHub Code Scanning,
 *               Azure DevOps, and other CI tools.
 *   pdf      -- printable PDF report (cover + summary + findings).
 */

#include "parson.h"
#include "policy.h"
#include "scan.h"
#include "format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdf_writer.h"

/* ----- CLI ----- */

enum OutputFormat {
	OUT_DEFAULT,
	OUT_SARIF,
	OUT_PDF
};

static void usage(FILE *out)
{
	fprintf(out,
"retrace-audit -- compliance audit report generator (TODO.complete/26)\n"
"\n"
"Usage:\n"
"  retrace-audit --policy POLICY.json --trace TRACE.json\n"
"                [--format default|sarif] [-o FINDINGS.json]\n"
"\n"
"Reads a retrace trace log, applies each policy rule's predicate\n"
"against each entry, and writes findings.\n"
"\n"
"Options:\n"
"  --policy PATH     Path to a policy JSON file (required)\n"
"  --trace  PATH     Path to a retrace JSON log (required)\n"
"  --format NAME     Output format: 'default' (custom JSON) or\n"
"                    'sarif' (SARIF 2.1.0 for GitHub Code Scanning,\n"
"                    Azure DevOps, etc.). Default: default.\n"
"  -o       PATH     Output path (default: stdout)\n"
"  --help           Show this message\n");
}

int main(int argc, char **argv)
{
	const char *policy_path = NULL;
	const char *trace_path = NULL;
	const char *out_path = NULL;
	enum OutputFormat fmt = OUT_DEFAULT;
	JSON_Value *trace_root;
	JSON_Array *trace;
	JSON_Value *out_root;
	struct Policy policy;
	struct Findings findings;
	FILE *out = stdout;
	int argi;

	for (argi = 1; argi < argc; argi++) {
		if (strcmp(argv[argi], "--policy") == 0 && argi + 1 < argc) {
			policy_path = argv[++argi];
		} else if (strcmp(argv[argi], "--trace") == 0 &&
			   argi + 1 < argc) {
			trace_path = argv[++argi];
		} else if (strcmp(argv[argi], "-o") == 0 &&
			   argi + 1 < argc) {
			out_path = argv[++argi];
		} else if (strcmp(argv[argi], "--format") == 0 &&
			   argi + 1 < argc) {
			const char *f = argv[++argi];

			if (strcmp(f, "default") == 0)
				fmt = OUT_DEFAULT;
			else if (strcmp(f, "sarif") == 0)
				fmt = OUT_SARIF;
			else if (strcmp(f, "pdf") == 0)
				fmt = OUT_PDF;
			else {
				fprintf(stderr,
					"retrace-audit: unknown format '%s'\n",
					f);
				usage(stderr);
				return 1;
			}
		} else if (strcmp(argv[argi], "--help") == 0 ||
			   strcmp(argv[argi], "-h") == 0) {
			usage(stdout);
			return 0;
		} else {
			fprintf(stderr,
				"retrace-audit: unknown arg '%s'\n", argv[argi]);
			usage(stderr);
			return 1;
		}
	}

	if (policy_path == NULL || trace_path == NULL) {
		fprintf(stderr,
			"retrace-audit: --policy and --trace required\n");
		usage(stderr);
		return 1;
	}

	if (policy_load_from_file(policy_path, &policy) != 0)
		return 1;

	trace_root = json_parse_file(trace_path);
	if (trace_root == NULL) {
		fprintf(stderr, "retrace-audit: cannot parse %s\n",
			trace_path);
		policy_free(&policy);
		return 1;
	}
	trace = json_value_get_array(trace_root);
	if (trace == NULL) {
		fprintf(stderr, "retrace-audit: %s is not a JSON array\n",
			trace_path);
		json_value_free(trace_root);
		policy_free(&policy);
		return 1;
	}

	audit_findings_init(&findings);
	audit_scan_trace(trace, &policy, &findings);

	if (fmt == OUT_PDF) {
		int nc = 0, nh = 0, nm = 0, ni = 0;
		size_t fi;
		const char **sevs = NULL;
		const char **ids = NULL;
		const char **descs = NULL;

		for (fi = 0; fi < findings.count; fi++) {
			switch (findings.items[fi].rule->severity) {
			case SEV_CRITICAL:
				nc++;
				break;
			case SEV_HIGH:
				nh++;
				break;
			case SEV_MEDIUM:
				nm++;
				break;
			default:
				ni++;
				break;
			}
		}
		if (findings.count > 0) {
			sevs = malloc(findings.count * sizeof(char *));
			ids = malloc(findings.count * sizeof(char *));
			descs = malloc(findings.count * sizeof(char *));
			for (fi = 0; fi < findings.count; fi++) {
				sevs[fi] = severity_str(
					findings.items[fi].rule->severity);
				ids[fi] = findings.items[fi].rule->id;
				descs[fi] =
					findings.items[fi].rule->description;
			}
		}
		if (out_path != NULL) {
			out = fopen(out_path, "wb");
			if (out == NULL) {
				fprintf(stderr,
					"retrace-audit: cannot open %s\n",
					out_path);
				free(sevs);
				free(ids);
				free(descs);
				json_value_free(trace_root);
				audit_findings_free(&findings);
				policy_free(&policy);
				return 1;
			}
		}
		pdf_write_audit_report(out, policy.name, trace_path,
			sevs, ids, descs, (int)findings.count,
			nc, nh, nm, ni);
		free(sevs);
		free(ids);
		free(descs);
		if (out != stdout)
			fclose(out);
		json_value_free(trace_root);
		audit_findings_free(&findings);
		policy_free(&policy);
		return 0;
	}

	switch (fmt) {
	case OUT_SARIF:
		out_root = audit_format_sarif(&policy, trace_path, &findings);
		break;
	case OUT_DEFAULT:
	default:
		out_root = audit_format_default(&policy, trace_path, &findings);
		break;
	}

	if (out_path != NULL) {
		out = fopen(out_path, "w");
		if (out == NULL) {
			fprintf(stderr, "retrace-audit: cannot open %s\n",
				out_path);
			json_value_free(out_root);
			json_value_free(trace_root);
			audit_findings_free(&findings);
			policy_free(&policy);
			return 1;
		}
	}

	{
		char *serialized = json_serialize_to_string_pretty(out_root);

		fputs(serialized, out);
		fputc('\n', out);
		json_free_serialized_string(serialized);
	}

	if (out != stdout)
		fclose(out);

	json_value_free(out_root);
	json_value_free(trace_root);
	audit_findings_free(&findings);
	policy_free(&policy);
	return 0;
}
