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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdf_writer.h"

/* ----- Severity mappings for SARIF ----- */

static const char *sarif_level(enum Severity s)
{
	switch (s) {
	case SEV_CRITICAL:
		return "error";
	case SEV_HIGH:
		return "error";
	case SEV_MEDIUM:
		return "warning";
	case SEV_INFO:
	default:
		return "note";
	}
}

/* ----- Formatters ----- */

static JSON_Value *format_default(const struct Policy *policy,
				  const char *trace_path,
				  const struct Findings *findings)
{
	JSON_Value *root = json_value_init_object();
	JSON_Object *obj = json_value_get_object(root);
	JSON_Array *arr;
	JSON_Object *summary;
	size_t i;

	json_object_set_string(obj, "policy", policy->name);
	json_object_set_string(obj, "trace", trace_path);

	arr = json_value_get_array(json_value_init_array());
	json_object_set_value(obj, "findings",
		json_array_get_wrapping_value(arr));

	for (i = 0; i < findings->count; i++) {
		const struct Finding *f = &findings->items[i];
		JSON_Value *fv = json_value_init_object();
		JSON_Object *fo = json_value_get_object(fv);
		JSON_Value *entry_copy;

		json_object_set_string(fo, "rule_id", f->rule->id);
		json_object_set_string(fo, "severity",
			severity_str(f->rule->severity));
		json_object_set_string(fo, "description", f->rule->description);
		json_object_set_number(fo, "entry_index",
			(double)f->entry_index);
		entry_copy = json_value_deep_copy(
			json_object_get_wrapping_value(f->entry));
		json_object_set_value(fo, "entry", entry_copy);
		json_array_append_value(arr, fv);
	}

	summary = json_value_get_object(json_value_init_object());
	json_object_set_value(obj, "summary",
		json_object_get_wrapping_value(summary));
	json_object_set_number(summary, "critical", 0);
	json_object_set_number(summary, "high", 0);
	json_object_set_number(summary, "medium", 0);
	json_object_set_number(summary, "info", 0);
	for (i = 0; i < findings->count; i++) {
		const char *sev = severity_str(findings->items[i].rule->severity);
		double cur = json_object_get_number(summary, sev);

		json_object_set_number(summary, sev, cur + 1);
	}

	return root;
}

/*
 * SARIF 2.1.0 format. Schema:
 * https://docs.oasis-open.org/sarif/sarif/v2.1.0/sarif-v2.1.0.html
 *
 * One run, one tool (retrace-audit). Each finding becomes a
 * result. ruleId is the policy rule's id; level is mapped from
 * severity (critical/high -> error, medium -> warning, info ->
 * note). The trace file is the artifactLocation; entry_index is
 * encoded as a region.startLine (1-based; SARIF doesn't have a
 * native "array index" concept, so we use line numbers as a
 * proxy).
 */
static JSON_Value *format_sarif(const struct Policy *policy,
				const char *trace_path,
				const struct Findings *findings)
{
	JSON_Value *root = json_value_init_object();
	JSON_Object *obj = json_value_get_object(root);
	JSON_Array *runs;
	JSON_Value *run_v;
	JSON_Object *run;
	JSON_Object *tool;
	JSON_Object *driver;
	JSON_Array *results;
	size_t i;

	json_object_set_string(obj, "$schema",
		"https://json.schemastore.org/sarif-2.1.0.json");
	json_object_set_string(obj, "version", "2.1.0");

	runs = json_value_get_array(json_value_init_array());
	json_object_set_value(obj, "runs",
		json_array_get_wrapping_value(runs));

	run_v = json_value_init_object();
	run = json_value_get_object(run_v);
	json_array_append_value(runs, run_v);

	tool = json_value_get_object(json_value_init_object());
	json_object_set_value(run, "tool",
		json_object_get_wrapping_value(tool));

	driver = json_value_get_object(json_value_init_object());
	json_object_set_value(tool, "driver",
		json_object_get_wrapping_value(driver));
	json_object_set_string(driver, "name", "retrace-audit");
	json_object_set_string(driver, "version", "0.1.0");
	json_object_set_string(driver, "informationUri",
		"https://github.com/riboseinc/retrace");

	results = json_value_get_array(json_value_init_array());
	json_object_set_value(run, "results",
		json_array_get_wrapping_value(results));

	for (i = 0; i < findings->count; i++) {
		const struct Finding *f = &findings->items[i];
		JSON_Value *rv = json_value_init_object();
		JSON_Object *r = json_value_get_object(rv);
		JSON_Object *msg;
		JSON_Object *loc;
		JSON_Object *phys;
		JSON_Object *art;
		JSON_Object *region;

		json_object_set_string(r, "ruleId", f->rule->id);
		json_object_set_string(r, "level", sarif_level(f->rule->severity));

		msg = json_value_get_object(json_value_init_object());
		json_object_set_value(r, "message",
			json_object_get_wrapping_value(msg));
		json_object_set_string(msg, "text", f->rule->description);

		loc = json_value_get_object(json_value_init_object());
		{
			JSON_Array *locs = json_value_get_array(
				json_value_init_array());

			json_object_set_value(r, "locations",
				json_array_get_wrapping_value(locs));
			json_array_append_value(locs,
				json_object_get_wrapping_value(loc));
		}

		phys = json_value_get_object(json_value_init_object());
		json_object_set_value(loc, "physicalLocation",
			json_object_get_wrapping_value(phys));

		art = json_value_get_object(json_value_init_object());
		json_object_set_value(phys, "artifactLocation",
			json_object_get_wrapping_value(art));
		json_object_set_string(art, "uri", trace_path);

		region = json_value_get_object(json_value_init_object());
		json_object_set_value(phys, "region",
			json_object_get_wrapping_value(region));
		/* SARIF region.startLine is 1-based; our entry_index
		 * is 0-based. Bump by 1 so the first entry maps to
		 * line 1.
		 */
		json_object_set_number(region, "startLine",
			(double)f->entry_index + 1);

		json_array_append_value(results, rv);
	}

	(void)policy;
	return root;
}

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
		out_root = format_sarif(&policy, trace_path, &findings);
		break;
	case OUT_DEFAULT:
	default:
		out_root = format_default(&policy, trace_path, &findings);
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
