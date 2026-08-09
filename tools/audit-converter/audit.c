/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-audit: compliance audit report generator
 * (TODO.complete/26 MVP).
 *
 * Reads a retrace JSON log, applies each policy rule's predicate
 * against each entry, and emits findings as JSON.
 *
 * Usage:
 *   retrace run --log /tmp/trace.json -- ./your-binary
 *   retrace-audit --policy share/policies/baseline.json \
 *                 --trace /tmp/trace.json > findings.json
 *
 * Output schema (one JSON object on stdout):
 *   {
 *     "policy": "baseline",
 *     "trace": "/tmp/trace.json",
 *     "summary": { "critical": N, "high": N, "medium": N, "info": N },
 *     "findings": [
 *       { "rule_id": "...", "severity": "high",
 *         "description": "...", "entry_index": N,
 *         "entry": { ... original log entry ... } }
 *     ]
 *   }
 *
 * PDF rendering, SARIF output, and the full filter-DSL predicates
 * land in follow-up PRs. This MVP covers the most common
 * compliance questions: PII path access, network connects,
 * subprocess spawning, sensitive env var reads.
 */

#include "parson.h"
#include "policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Predicate evaluation.
 *
 * The entry is the log entry's "message" object (the part that
 * varies by action). For log_params entries, "func" is the
 * intercepted function name and other fields are the parsed
 * arguments (path, buf, etc.).
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
 * Emit a finding as a JSON object. The finding includes:
 *   - rule_id, severity, description from the rule
 *   - entry_index (0-based position in the log array)
 *   - entry (the original log entry, for evidence)
 */
static void emit_finding(const struct Rule *rule, size_t entry_idx,
			 JSON_Object *original_entry, JSON_Array *findings)
{
	JSON_Value *finding;
	JSON_Object *obj;

	finding = json_value_init_object();
	obj = json_value_get_object(finding);

	json_object_set_string(obj, "rule_id", rule->id);
	json_object_set_string(obj, "severity", severity_str(rule->severity));
	json_object_set_string(obj, "description", rule->description);
	json_object_set_number(obj, "entry_index", (double)entry_idx);

	/* Deep-copy the original entry so the caller can free its
	 * root value without invalidating the finding's reference.
	 */
	{
		JSON_Value *entry_copy = json_value_deep_copy(
			json_object_get_wrapping_value(original_entry));

		json_object_set_value(obj, "entry", entry_copy);
	}

	json_array_append_value(findings, finding);
}

static void usage(FILE *out)
{
	fprintf(out,
"retrace-audit -- compliance audit report generator (TODO.complete/26 MVP)\n"
"\n"
"Usage:\n"
"  retrace-audit --policy POLICY.json --trace TRACE.json [-o FINDINGS.json]\n"
"\n"
"Reads a retrace trace log, applies each policy rule's predicate\n"
"against each entry, and writes findings as JSON. Default output\n"
"is stdout; use -o to write to a file.\n"
"\n"
"Options:\n"
"  --policy PATH   Path to a policy JSON file (required)\n"
"  --trace  PATH   Path to a retrace JSON log (required)\n"
"  -o       PATH   Output path (default: stdout)\n"
"  --help         Show this message\n");
}

int main(int argc, char **argv)
{
	const char *policy_path = NULL;
	const char *trace_path = NULL;
	const char *out_path = NULL;
	JSON_Value *trace_root;
	JSON_Array *trace;
	JSON_Value *out_root;
	JSON_Object *out_obj;
	JSON_Array *findings;
	JSON_Object *summary;
	struct Policy policy;
	size_t i, n;
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

	out_root = json_value_init_object();
	out_obj = json_value_get_object(out_root);
	json_object_set_string(out_obj, "policy", policy.name);
	json_object_set_string(out_obj, "trace", trace_path);

	findings = json_value_get_array(json_value_init_array());
	json_object_set_value(out_obj, "findings",
		json_array_get_wrapping_value(findings));

	summary = json_value_get_object(json_value_init_object());
	json_object_set_value(out_obj, "summary",
		json_object_get_wrapping_value(summary));
	json_object_set_number(summary, "critical", 0);
	json_object_set_number(summary, "high", 0);
	json_object_set_number(summary, "medium", 0);
	json_object_set_number(summary, "info", 0);

	/* Apply each rule against each trace entry. */
	n = json_array_get_count(trace);
	for (i = 0; i < n; i++) {
		JSON_Object *entry = json_array_get_object(trace, i);
		JSON_Object *msg;
		size_t r;

		if (entry == NULL)
			continue;
		msg = json_object_get_object(entry, "message");
		if (msg == NULL)
			continue;

		for (r = 0; r < policy.rules_count; r++) {
			struct Rule *rule = &policy.rules[r];

			if (rule_matches(rule, msg)) {
				emit_finding(rule, i, msg, findings);

				switch (rule->severity) {
				case SEV_CRITICAL:
					json_object_set_number(summary,
						"critical",
						json_object_get_number(summary,
							"critical") + 1);
					break;
				case SEV_HIGH:
					json_object_set_number(summary,
						"high",
						json_object_get_number(summary,
							"high") + 1);
					break;
				case SEV_MEDIUM:
					json_object_set_number(summary,
						"medium",
						json_object_get_number(summary,
							"medium") + 1);
					break;
				case SEV_INFO:
				default:
					json_object_set_number(summary,
						"info",
						json_object_get_number(summary,
							"info") + 1);
					break;
				}
			}
		}
	}

	if (out_path != NULL) {
		out = fopen(out_path, "w");
		if (out == NULL) {
			fprintf(stderr, "retrace-audit: cannot open %s\n",
				out_path);
			json_value_free(out_root);
			json_value_free(trace_root);
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
	policy_free(&policy);
	return 0;
}
