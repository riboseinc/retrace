/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the audit scan engine (TODO.complete/26).
 *
 * audit_scan_trace is the loop that drives the policy matcher
 * against every trace entry and collects findings. Wrong behavior
 * here means missing violations (false negatives in compliance
 * reports) or duplicate/phantom findings.
 *
 * Covers:
 *   - single rule matching a single entry
 *   - severity preserved through finding->rule
 *   - findings appear in trace order; within one entry, in
 *     policy-rule order
 *   - multiple rules can match the same entry
 *   - the same rule can match multiple entries
 *   - entries without a "message" object are skipped
 *   - empty trace yields zero findings
 *   - policy with zero rules yields zero findings
 *   - non-matching trace yields zero findings
 *   - findings_append grows the array past its initial capacity
 *   - init/free lifecycle (free then init is reusable)
 */

#include "parson.h"
#include "policy.h"
#include "scan.h"

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

/* Build a policy from a JSON source string. Caller frees. */
static struct Policy *policy_from(const char *src)
{
	static struct Policy p;
	JSON_Value *v = json_parse_string(src);
	int rc;

	rc = policy_load_from_json(json_value_get_object(v), &p);
	if (rc != 0)
		return NULL;
	return &p;
}

static void policy_free_local(struct Policy *p)
{
	policy_free(p);
}

/* Build a trace array from a JSON source string. Caller frees
 * via json_value_free(json_array_get_wrapping_value(a)).
 */
static JSON_Array *trace_from(const char *src)
{
	return json_value_get_array(json_parse_string(src));
}

static void trace_free_local(JSON_Array *a)
{
	json_value_free(json_array_get_wrapping_value(a));
}

/* ----- basic matching ----- */

static void test_single_rule_single_entry(void)
{
	struct Policy *p = policy_from(
		"{\"name\":\"t\",\"rules\":["
		"{\"id\":\"R1\",\"description\":\"d\","
		"\"match\":{\"func_exact\":\"system\"},\"severity\":\"high\"}"
		"]}");
	JSON_Array *t = trace_from(
		"[{\"message\":{\"func\":\"system\",\"args\":{}}}]");
	struct Findings f;

	CHECK(p != NULL);
	audit_findings_init(&f);
	audit_scan_trace(t, p, &f);

	CHECK(f.count == 1);
	CHECK(strcmp(f.items[0].rule->id, "R1") == 0);
	CHECK(f.items[0].entry_index == 0);
	CHECK(f.items[0].entry != NULL);

	audit_findings_free(&f);
	trace_free_local(t);
	policy_free_local(p);
}

static void test_severity_preserved_through_rule(void)
{
	struct Policy *p = policy_from(
		"{\"name\":\"t\",\"rules\":["
		"{\"id\":\"R1\",\"description\":\"d\","
		"\"match\":{\"func_exact\":\"open\"},\"severity\":\"critical\"}"
		"]}");
	JSON_Array *t = trace_from(
		"[{\"message\":{\"func\":\"open\",\"args\":{}}}]");
	struct Findings f;

	CHECK(p != NULL);
	audit_findings_init(&f);
	audit_scan_trace(t, p, &f);

	CHECK(f.count == 1);
	CHECK(f.items[0].rule->severity == SEV_CRITICAL);
	CHECK(strcmp(severity_str(f.items[0].rule->severity),
		"critical") == 0);

	audit_findings_free(&f);
	trace_free_local(t);
	policy_free_local(p);
}

/* ----- ordering ----- */

static void test_findings_in_trace_order(void)
{
	struct Policy *p = policy_from(
		"{\"name\":\"t\",\"rules\":["
		"{\"id\":\"R\",\"description\":\"d\","
		"\"match\":{\"func_prefix\":\"get\"},\"severity\":\"info\"}"
		"]}");
	/* Entries 0 and 2 match; entry 1 does not. */
	JSON_Array *t = trace_from(
		"[{\"message\":{\"func\":\"getenv\"}},"
		"{\"message\":{\"func\":\"setenv\"}},"
		"{\"message\":{\"func\":\"getcwd\"}}]");
	struct Findings f;

	CHECK(p != NULL);
	audit_findings_init(&f);
	audit_scan_trace(t, p, &f);

	CHECK(f.count == 2);
	CHECK(f.items[0].entry_index == 0);
	CHECK(f.items[1].entry_index == 2);

	audit_findings_free(&f);
	trace_free_local(t);
	policy_free_local(p);
}

static void test_within_entry_policy_rule_order(void)
{
	/* Two rules both match the same entry: findings must appear
	 * in policy-rule order (R1 before R2).
	 */
	struct Policy *p = policy_from(
		"{\"name\":\"t\",\"rules\":["
		"{\"id\":\"R1\",\"description\":\"d1\","
		"\"match\":{\"func_exact\":\"open\"},\"severity\":\"info\"},"
		"{\"id\":\"R2\",\"description\":\"d2\","
		"\"match\":{\"func_prefix\":\"op\"},\"severity\":\"info\"}"
		"]}");
	JSON_Array *t = trace_from(
		"[{\"message\":{\"func\":\"open\",\"args\":{}}}]");
	struct Findings f;

	CHECK(p != NULL);
	audit_findings_init(&f);
	audit_scan_trace(t, p, &f);

	CHECK(f.count == 2);
	CHECK(strcmp(f.items[0].rule->id, "R1") == 0);
	CHECK(strcmp(f.items[1].rule->id, "R2") == 0);

	audit_findings_free(&f);
	trace_free_local(t);
	policy_free_local(p);
}

static void test_same_rule_matches_multiple_entries(void)
{
	struct Policy *p = policy_from(
		"{\"name\":\"t\",\"rules\":["
		"{\"id\":\"R\",\"description\":\"d\","
		"\"match\":{\"func_exact\":\"open\"},\"severity\":\"info\"}"
		"]}");
	JSON_Array *t = trace_from(
		"[{\"message\":{\"func\":\"open\"}},"
		"{\"message\":{\"func\":\"read\"}},"
		"{\"message\":{\"func\":\"open\"}}]");
	struct Findings f;

	CHECK(p != NULL);
	audit_findings_init(&f);
	audit_scan_trace(t, p, &f);

	CHECK(f.count == 2);
	CHECK(f.items[0].entry_index == 0);
	CHECK(f.items[1].entry_index == 2);

	audit_findings_free(&f);
	trace_free_local(t);
	policy_free_local(p);
}

/* ----- skips and empties ----- */

static void test_entries_without_message_skipped(void)
{
	struct Policy *p = policy_from(
		"{\"name\":\"t\",\"rules\":["
		"{\"id\":\"R\",\"description\":\"d\","
		"\"match\":{\"path_contains\":\"secret\"},\"severity\":\"info\"}"
		"]}");
	/* Entry 0 has a message that matches; entry 1 has no
	 * message object at all and must be skipped by the scanner
	 * regardless of the rule.
	 */
	JSON_Array *t = trace_from(
		"[{\"message\":{\"path\":\"/etc/secret.key\"}},"
		"{\"nomessage\":1}]");
	struct Findings f;

	CHECK(p != NULL);
	audit_findings_init(&f);
	audit_scan_trace(t, p, &f);

	CHECK(f.count == 1);
	CHECK(f.items[0].entry_index == 0);

	audit_findings_free(&f);
	trace_free_local(t);
	policy_free_local(p);
}

static void test_empty_trace_zero_findings(void)
{
	struct Policy *p = policy_from(
		"{\"name\":\"t\",\"rules\":["
		"{\"id\":\"R\",\"description\":\"d\","
		"\"match\":{\"func_prefix\":\"\"},\"severity\":\"info\"}"
		"]}");
	JSON_Array *t = trace_from("[]");
	struct Findings f;

	CHECK(p != NULL);
	audit_findings_init(&f);
	audit_scan_trace(t, p, &f);
	CHECK(f.count == 0);
	CHECK(f.items == NULL);

	audit_findings_free(&f);
	trace_free_local(t);
	policy_free_local(p);
}

static void test_zero_rule_policy_zero_findings(void)
{
	struct Policy *p = policy_from("{\"name\":\"t\"}");
	JSON_Array *t = trace_from(
		"[{\"message\":{\"func\":\"open\"}}]");
	struct Findings f;

	CHECK(p != NULL);
	CHECK(p->rules_count == 0);
	audit_findings_init(&f);
	audit_scan_trace(t, p, &f);
	CHECK(f.count == 0);

	audit_findings_free(&f);
	trace_free_local(t);
	policy_free_local(p);
}

static void test_no_match_zero_findings(void)
{
	struct Policy *p = policy_from(
		"{\"name\":\"t\",\"rules\":["
		"{\"id\":\"R\",\"description\":\"d\","
		"\"match\":{\"func_exact\":\"execve\"},\"severity\":\"high\"}"
		"]}");
	JSON_Array *t = trace_from(
		"[{\"message\":{\"func\":\"open\"}},"
		"{\"message\":{\"func\":\"read\"}}]");
	struct Findings f;

	CHECK(p != NULL);
	audit_findings_init(&f);
	audit_scan_trace(t, p, &f);
	CHECK(f.count == 0);

	audit_findings_free(&f);
	trace_free_local(t);
	policy_free_local(p);
}

/* ----- findings_append growth ----- */

static void test_append_grows_past_initial_capacity(void)
{
	/* Initial capacity is 16; append 100 and verify all land. */
	struct Findings f;
	struct Policy *p = policy_from(
		"{\"name\":\"t\",\"rules\":["
		"{\"id\":\"R\",\"description\":\"d\","
		"\"match\":{\"func_prefix\":\"\"},\"severity\":\"info\"}"
		"]}");
	JSON_Array *t = trace_from("[]");
	int i;
	int ok = 1;

	CHECK(p != NULL);
	audit_findings_init(&f);
	for (i = 0; i < 100; i++) {
		if (audit_findings_append(&f, &p->rules[0],
			(size_t)i, NULL) != 0) {
			ok = 0;
			break;
		}
	}
	CHECK(ok == 1);
	CHECK(f.count == 100);
	CHECK(f.cap >= 100);
	/* Last append landed with the right index. */
	CHECK(f.items[99].entry_index == 99);

	audit_findings_free(&f);
	trace_free_local(t);
	policy_free_local(p);
}

static void test_init_after_free_reusable(void)
{
	struct Findings f;

	audit_findings_init(&f);
	CHECK(audit_findings_append(&f, NULL, 0, NULL) == 0);
	audit_findings_free(&f);
	CHECK(f.items == NULL);
	CHECK(f.count == 0);
	CHECK(f.cap == 0);

	audit_findings_init(&f);
	CHECK(f.count == 0);
	CHECK(audit_findings_append(&f, NULL, 7, NULL) == 0);
	CHECK(f.items[0].entry_index == 7);
	audit_findings_free(&f);
}

int main(void)
{
	printf("-- basic matching --\n");
	TEST(single_rule_single_entry);
	TEST(severity_preserved_through_rule);

	printf("-- ordering --\n");
	TEST(findings_in_trace_order);
	TEST(within_entry_policy_rule_order);
	TEST(same_rule_matches_multiple_entries);

	printf("-- skips and empties --\n");
	TEST(entries_without_message_skipped);
	TEST(empty_trace_zero_findings);
	TEST(zero_rule_policy_zero_findings);
	TEST(no_match_zero_findings);

	printf("-- findings_append growth + lifecycle --\n");
	TEST(append_grows_past_initial_capacity);
	TEST(init_after_free_reusable);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
