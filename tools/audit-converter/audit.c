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
 *
 * PDF rendering is out of scope here (TODO.complete/26 P2 -- the
 * "tiny PDF writer" sub-task). This module owns the data model
 * + JSON/SARIF serialization.
 */

#include "parson.h"
#include "policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Predicate evaluation. The entry is the log entry's "message"
 * object (the part that varies by action). For log_params
 * entries, "func" is the intercepted function name and other
 * fields are the parsed arguments (path, buf, etc.).
 *
 * Predicate semantics: a rule matches if ALL of its non-NULL
 * constraints match (AND). NULL constraints are wildcards.
 *
 * Returns 1 if the rule matches, 0 otherwise.
 */
static int rule_matches(const struct Rule *rule, JSON_Object *msg)
{
	const char *func;
	size_t nkeys, k;

	if (rule == NULL || msg == NULL)
		return 0;

	func = json_object_get_string(msg, "func");

	if (rule->func_exact != NULL) {
		if (func == NULL || strcmp(func, rule->func_exact) != 0)
			return 0;
	}

	if (rule->func_prefix != NULL) {
		size_t plen = strlen(rule->func_prefix);

		if (func == NULL || strncmp(func, rule->func_prefix, plen) != 0)
			return 0;
	}

	/* path_contains: scan every string value in the message */
	if (rule->path_contains != NULL) {
		int found = 0;

		nkeys = json_object_get_count(msg);
		for (k = 0; k < nkeys; k++) {
			JSON_Value *v = json_object_get_value_at(msg, k);
			const char *s;

			if (json_value_get_type(v) != JSONString)
				continue;
			s = json_value_get_string(v);
			if (s != NULL && strstr(s, rule->path_contains) != NULL) {
				found = 1;
				break;
			}
		}
		if (!found)
			return 0;
	}

	/* env_pattern: glob match against getenv entries.
	 * Patterns supported:
	 *   *_TOKEN, *_KEY, *_PASSWORD  (suffix)
	 *   LD_*, IFS                   (prefix)
	 *   (anything else is exact)
	 */
	if (rule->env_pattern != NULL) {
		const char *var = json_object_get_string(msg, "name");
		size_t plen;
		int match = 0;

		if (var == NULL)
			return 0;
		plen = strlen(rule->env_pattern);
		if (rule->env_pattern[0] == '*' && plen > 1) {
			/* Suffix match: *_TOKEN */
			size_t vlen = strlen(var);
			size_t suffix_len = plen - 1;

			if (vlen >= suffix_len &&
			    strcmp(var + vlen - suffix_len,
				   rule->env_pattern + 1) == 0)
				match = 1;
		} else if (plen > 0 && rule->env_pattern[plen - 1] == '*') {
			/* Prefix match: LD_* */
			if (strncmp(var, rule->env_pattern, plen - 1) == 0)
				match = 1;
		} else {
			if (strcmp(var, rule->env_pattern) == 0)
				match = 1;
		}
		if (!match)
			return 0;
	}

	return 1;
}

/*
 * Internal finding representation. Decoupled from the on-disk
 * format so we can add formatters (default, SARIF, future PDF
 * text) without touching the matcher.
 */
struct Finding {
	const struct Rule *rule;

	size_t entry_index;

	JSON_Object *entry;  /* borrowed from trace_root; do not free */
};

struct Findings {
	struct Finding *items;

	size_t count;

	size_t cap;
};

static void findings_init(struct Findings *f)
{
	f->items = NULL;
	f->count = 0;
	f->cap = 0;
}

static void findings_free(struct Findings *f)
{
	free(f->items);
	f->items = NULL;
	f->count = 0;
	f->cap = 0;
}

static int findings_append(struct Findings *f, const struct Rule *rule,
			   size_t entry_index, JSON_Object *entry)
{
	struct Finding *newbuf;

	if (f->count == f->cap) {
		size_t newcap = (f->cap == 0) ? 16 : f->cap * 2;

		newbuf = (struct Finding *)realloc(f->items,
			newcap * sizeof(*newbuf));
		if (newbuf == NULL)
			return -1;
		f->items = newbuf;
		f->cap = newcap;
	}
	f->items[f->count].rule = rule;
	f->items[f->count].entry_index = entry_index;
	f->items[f->count].entry = entry;
	f->count++;
	return 0;
}

/*
 * Walks every (entry, rule) pair, appends matches to findings.
 */
static void scan_trace(JSON_Array *trace, const struct Policy *policy,
		       struct Findings *out)
{
	size_t i, n = json_array_get_count(trace);

	for (i = 0; i < n; i++) {
		JSON_Object *entry = json_array_get_object(trace, i);
		JSON_Object *msg;
		size_t r;

		if (entry == NULL)
			continue;
		msg = json_object_get_object(entry, "message");
		if (msg == NULL)
			continue;

		for (r = 0; r < policy->rules_count; r++) {
			const struct Rule *rule = &policy->rules[r];

			if (rule_matches(rule, msg))
				findings_append(out, rule, i, msg);
		}
	}
}

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
	OUT_SARIF
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

	findings_init(&findings);
	scan_trace(trace, &policy, &findings);

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
			findings_free(&findings);
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
	findings_free(&findings);
	policy_free(&policy);
	return 0;
}
