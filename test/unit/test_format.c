/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the audit output formatters (TODO.complete/26).
 *
 * audit_format_sarif is what GitHub Code Scanning and Azure DevOps
 * ingest natively -- a structural regression here breaks security
 * alert ingestion silently (the upload succeeds; the alerts never
 * render). These tests pin the SARIF 2.1.0 skeleton and every
 * field Code Scanning keys off:
 *   version, $schema, runs[0].tool.driver.name,
 *   results[].ruleId / level / message.text / region.startLine.
 *
 * audit_format_default is the human report: policy name, trace
 * path, per-finding evidence (deep-copied entry), and the summary
 * counts. Pinned here for the same reason.
 *
 * Covers:
 *   - audit_sarif_level: all four severities -> error/warning/note
 *   - SARIF skeleton (version, $schema, single run, driver name)
 *   - SARIF ruleId + level per finding
 *   - SARIF message.text from rule description
 *   - SARIF region.startLine = entry_index + 1 (1-based)
 *   - SARIF artifactLocation.uri = trace path
 *   - SARIF zero findings -> empty results array
 *   - default: policy + trace fields
 *   - default: finding fields incl. deep-copied entry evidence
 *   - default: summary counts per severity
 *   - default: zero findings -> zeroed summary
 */

#include "parson.h"
#include "policy.h"
#include "scan.h"
#include "format.h"

#include <stdio.h>
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

/* Fixture: one policy with one rule per severity, and one trace
 * entry that matches all of them (func_exact "open" + prefix
 * "op" both hit a func named "open").
 */
static const char *policy_src =
	"{\"name\":\"fmt-test\",\"rules\":["
	"{\"id\":\"CRIT\",\"description\":\"crit rule\","
	"\"match\":{\"func_exact\":\"open\"},\"severity\":\"critical\"},"
	"{\"id\":\"HIGH\",\"description\":\"high rule\","
	"\"match\":{\"func_prefix\":\"op\"},\"severity\":\"high\"},"
	"{\"id\":\"MED\",\"description\":\"med rule\","
	"\"match\":{\"path_contains\":\"secret\"},\"severity\":\"medium\"},"
	"{\"id\":\"INFO\",\"description\":\"info rule\","
	"\"match\":{\"env_pattern\":\"*_TOKEN\"},\"severity\":\"info\"}"
	"]}";

static const char *trace_src =
	"[{\"message\":{\"func\":\"open\",\"path\":\"/etc/secret.key\"}}]";

struct Fixture {
	struct Policy policy;
	JSON_Value *trace_v;
	JSON_Array *trace;
	struct Findings findings;
};

static int fixture_setup(struct Fixture *fx)
{
	JSON_Value *pv = json_parse_string(policy_src);
	int rc;

	memset(fx, 0, sizeof(*fx));
	rc = policy_load_from_json(json_value_get_object(pv), &fx->policy);
	json_value_free(pv);
	if (rc != 0)
		return -1;

	fx->trace_v = json_parse_string(trace_src);
	fx->trace = json_value_get_array(fx->trace_v);
	if (fx->trace == NULL)
		return -1;

	audit_findings_init(&fx->findings);
	audit_scan_trace(fx->trace, &fx->policy, &fx->findings);
	return 0;
}

static void fixture_teardown(struct Fixture *fx)
{
	audit_findings_free(&fx->findings);
	json_value_free(fx->trace_v);
	policy_free(&fx->policy);
}

/* ----- audit_sarif_level ----- */

static void test_sarif_level_mapping(void)
{
	CHECK(strcmp(audit_sarif_level(SEV_CRITICAL), "error") == 0);
	CHECK(strcmp(audit_sarif_level(SEV_HIGH), "error") == 0);
	CHECK(strcmp(audit_sarif_level(SEV_MEDIUM), "warning") == 0);
	CHECK(strcmp(audit_sarif_level(SEV_INFO), "note") == 0);
	CHECK(strcmp(audit_sarif_level((enum Severity)42), "note") == 0);
}

/* ----- SARIF skeleton ----- */

static void test_sarif_skeleton(void)
{
	struct Fixture fx;
	JSON_Value *out;
	JSON_Object *root;
	JSON_Array *runs;
	JSON_Object *run;
	JSON_Object *driver;

	CHECK(fixture_setup(&fx) == 0);
	out = audit_format_sarif(&fx.policy, "/t.json", &fx.findings);
	CHECK(out != NULL);
	root = json_value_get_object(out);

	CHECK(strcmp(json_object_get_string(root, "version"), "2.1.0") == 0);
	CHECK(strstr(json_object_get_string(root, "$schema"),
		"sarif-2.1.0.json") != NULL);

	runs = json_object_get_array(root, "runs");
	CHECK(json_array_get_count(runs) == 1);
	run = json_array_get_object(runs, 0);
	CHECK(run != NULL);

	driver = json_object_get_object(
		json_object_get_object(run, "tool"), "driver");
	CHECK(driver != NULL);
	CHECK(strcmp(json_object_get_string(driver, "name"),
		"retrace-audit") == 0);

	json_value_free(out);
	fixture_teardown(&fx);
}

/* ----- SARIF per-finding fields ----- */

static void test_sarif_ruleid_and_level(void)
{
	struct Fixture fx;
	JSON_Value *out;
	JSON_Array *results;

	CHECK(fixture_setup(&fx) == 0);
	/* func "open" matches CRIT (exact) and HIGH (prefix);
	 * path contains "secret" matches MED. INFO (env) does not.
	 */
	CHECK(fx.findings.count == 3);

	out = audit_format_sarif(&fx.policy, "/t.json", &fx.findings);
	{
		JSON_Object *root = json_value_get_object(out);
		JSON_Array *runs = json_object_get_array(root, "runs");
		JSON_Object *run = json_array_get_object(runs, 0);
		JSON_Array *results = json_object_get_array(run, "results");

		CHECK(json_array_get_count(results) == 3);

		CHECK(strcmp(json_object_get_string(
			json_array_get_object(results, 0), "ruleId"),
			"CRIT") == 0);
		CHECK(strcmp(json_object_get_string(
			json_array_get_object(results, 0), "level"),
			"error") == 0);
		CHECK(strcmp(json_object_get_string(
			json_array_get_object(results, 1), "ruleId"),
			"HIGH") == 0);
		CHECK(strcmp(json_object_get_string(
			json_array_get_object(results, 1), "level"),
			"error") == 0);
		CHECK(strcmp(json_object_get_string(
			json_array_get_object(results, 2), "ruleId"),
			"MED") == 0);
		CHECK(strcmp(json_object_get_string(
			json_array_get_object(results, 2), "level"),
			"warning") == 0);
	}

	json_value_free(out);
	fixture_teardown(&fx);
}

static void test_sarif_message_text(void)
{
	struct Fixture fx;
	JSON_Value *out;
	JSON_Object *root;
	JSON_Array *runs;
	JSON_Object *run;
	JSON_Array *results;
	JSON_Object *msg;

	CHECK(fixture_setup(&fx) == 0);
	out = audit_format_sarif(&fx.policy, "/t.json", &fx.findings);
	root = json_value_get_object(out);
	runs = json_object_get_array(root, "runs");
	run = json_array_get_object(runs, 0);
	results = json_object_get_array(run, "results");

	msg = json_object_get_object(json_array_get_object(results, 0),
		"message");
	CHECK(msg != NULL);
	CHECK(strcmp(json_object_get_string(msg, "text"),
		"crit rule") == 0);

	json_value_free(out);
	fixture_teardown(&fx);
}

static void test_sarif_startline_is_one_based(void)
{
	struct Fixture fx;
	JSON_Value *out;
	JSON_Object *root;
	JSON_Array *runs;
	JSON_Object *run;
	JSON_Array *results;
	JSON_Object *loc;
	JSON_Object *phys;
	JSON_Object *region;
	size_t i;

	CHECK(fixture_setup(&fx) == 0);
	out = audit_format_sarif(&fx.policy, "/t.json", &fx.findings);
	root = json_value_get_object(out);
	runs = json_object_get_array(root, "runs");
	run = json_array_get_object(runs, 0);
	results = json_object_get_array(run, "results");

	for (i = 0; i < json_array_get_count(results); i++) {
		loc = json_array_get_object(json_object_get_array(
			json_array_get_object(results, i), "locations"), 0);
		CHECK(loc != NULL);
		phys = json_object_get_object(loc, "physicalLocation");
		CHECK(phys != NULL);
		region = json_object_get_object(phys, "region");
		CHECK(region != NULL);
		/* Entry index 0 -> startLine 1. */
		CHECK(json_object_get_number(region, "startLine") == 1.0);
	}

	json_value_free(out);
	fixture_teardown(&fx);
}

static void test_sarif_artifact_uri(void)
{
	struct Fixture fx;
	JSON_Value *out;
	JSON_Object *root;
	JSON_Array *runs;
	JSON_Object *run;
	JSON_Array *results;
	JSON_Object *loc;
	JSON_Object *phys;
	JSON_Object *art;

	CHECK(fixture_setup(&fx) == 0);
	out = audit_format_sarif(&fx.policy, "/my/trace.json", &fx.findings);
	root = json_value_get_object(out);
	runs = json_object_get_array(root, "runs");
	run = json_array_get_object(runs, 0);
	results = json_object_get_array(run, "results");
	CHECK(json_array_get_count(results) > 0);

	loc = json_array_get_object(json_object_get_array(
		json_array_get_object(results, 0), "locations"), 0);
	CHECK(loc != NULL);
	phys = json_object_get_object(loc, "physicalLocation");
	CHECK(phys != NULL);
	art = json_object_get_object(phys, "artifactLocation");
	CHECK(art != NULL);
	CHECK(strcmp(json_object_get_string(art, "uri"),
		"/my/trace.json") == 0);

	json_value_free(out);
	fixture_teardown(&fx);
}

static void test_sarif_zero_findings_empty_results(void)
{
	struct Fixture fx;
	JSON_Value *out;
	JSON_Object *root;
	JSON_Array *runs;
	JSON_Object *run;
	JSON_Array *results;

	CHECK(fixture_setup(&fx) == 0);
	audit_findings_free(&fx.findings);
	audit_findings_init(&fx.findings);

	out = audit_format_sarif(&fx.policy, "/t.json", &fx.findings);
	root = json_value_get_object(out);
	runs = json_object_get_array(root, "runs");
	run = json_array_get_object(runs, 0);
	results = json_object_get_array(run, "results");
	CHECK(json_array_get_count(results) == 0);

	json_value_free(out);
	fixture_teardown(&fx);
}

/* ----- default format ----- */

static void test_default_policy_and_trace_fields(void)
{
	struct Fixture fx;
	JSON_Value *out;
	JSON_Object *root;

	CHECK(fixture_setup(&fx) == 0);
	out = audit_format_default(&fx.policy, "/t.json", &fx.findings);
	root = json_value_get_object(out);

	CHECK(strcmp(json_object_get_string(root, "policy"),
		"fmt-test") == 0);
	CHECK(strcmp(json_object_get_string(root, "trace"),
		"/t.json") == 0);

	json_value_free(out);
	fixture_teardown(&fx);
}

static void test_default_finding_fields_and_evidence(void)
{
	struct Fixture fx;
	JSON_Value *out;
	JSON_Object *root;
	JSON_Array *arr;
	JSON_Object *f0;
	JSON_Object *entry;

	CHECK(fixture_setup(&fx) == 0);
	out = audit_format_default(&fx.policy, "/t.json", &fx.findings);
	root = json_value_get_object(out);
	arr = json_object_get_array(root, "findings");
	CHECK(json_array_get_count(arr) == 3);

	f0 = json_array_get_object(arr, 0);
	CHECK(strcmp(json_object_get_string(f0, "rule_id"), "CRIT") == 0);
	CHECK(strcmp(json_object_get_string(f0, "severity"),
		"critical") == 0);
	CHECK(strcmp(json_object_get_string(f0, "description"),
		"crit rule") == 0);
	CHECK(json_object_get_number(f0, "entry_index") == 0.0);

	/* Evidence: deep copy of the triggering entry. */
	entry = json_object_get_object(f0, "entry");
	CHECK(entry != NULL);
	CHECK(strcmp(json_object_get_string(entry, "func"), "open") == 0);
	CHECK(strstr(json_object_get_string(entry, "path"),
		"secret") != NULL);

	json_value_free(out);
	fixture_teardown(&fx);
}

static void test_default_summary_counts(void)
{
	struct Fixture fx;
	JSON_Value *out;
	JSON_Object *root;
	JSON_Object *summary;

	CHECK(fixture_setup(&fx) == 0);
	out = audit_format_default(&fx.policy, "/t.json", &fx.findings);
	root = json_value_get_object(out);
	summary = json_object_get_object(root, "summary");

	CHECK(json_object_get_number(summary, "critical") == 1.0);
	CHECK(json_object_get_number(summary, "high") == 1.0);
	CHECK(json_object_get_number(summary, "medium") == 1.0);
	CHECK(json_object_get_number(summary, "info") == 0.0);

	json_value_free(out);
	fixture_teardown(&fx);
}

static void test_default_zero_findings_zeroed_summary(void)
{
	struct Fixture fx;
	JSON_Value *out;
	JSON_Object *root;
	JSON_Object *summary;
	JSON_Array *arr;

	CHECK(fixture_setup(&fx) == 0);
	audit_findings_free(&fx.findings);
	audit_findings_init(&fx.findings);

	out = audit_format_default(&fx.policy, "/t.json", &fx.findings);
	root = json_value_get_object(out);
	arr = json_object_get_array(root, "findings");
	CHECK(json_array_get_count(arr) == 0);

	summary = json_object_get_object(root, "summary");
	CHECK(json_object_get_number(summary, "critical") == 0.0);
	CHECK(json_object_get_number(summary, "high") == 0.0);
	CHECK(json_object_get_number(summary, "medium") == 0.0);
	CHECK(json_object_get_number(summary, "info") == 0.0);

	json_value_free(out);
	fixture_teardown(&fx);
}

int main(void)
{
	printf("-- audit_sarif_level --\n");
	TEST(sarif_level_mapping);

	printf("-- SARIF skeleton --\n");
	TEST(sarif_skeleton);

	printf("-- SARIF per-finding fields --\n");
	TEST(sarif_ruleid_and_level);
	TEST(sarif_message_text);
	TEST(sarif_startline_is_one_based);
	TEST(sarif_artifact_uri);

	printf("-- SARIF zero findings --\n");
	TEST(sarif_zero_findings_empty_results);

	printf("-- default format --\n");
	TEST(default_policy_and_trace_fields);
	TEST(default_finding_fields_and_evidence);
	TEST(default_summary_counts);
	TEST(default_zero_findings_zeroed_summary);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
