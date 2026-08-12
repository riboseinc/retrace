/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_AUDIT_POLICY_H_
#define RETRACE_AUDIT_POLICY_H_

#include "parson.h"

#include <stddef.h>

/*
 * Compliance audit policy (TODO.complete/26 MVP).
 *
 * A policy is a list of rules. Each rule has:
 *   - id          : stable identifier (e.g. "SOC2-CC6.1")
 *   - description : human-readable summary
 *   - match       : predicate evaluated against each log entry
 *   - severity    : "info" / "medium" / "high" / "critical"
 *
 * Predicate shapes (all optional; missing = match-all):
 *   func_prefix  : entry's message.func starts with this string
 *   func_exact   : entry's message.func equals this string
 *   path_contains: any string param value contains this substring
 *                   (case-sensitive)
 *   env_pattern  : getenv calls where the var name matches a
 *                   glob-style pattern (*_TOKEN, *_KEY, etc.)
 *
 * MVP predicates cover the most common compliance questions:
 *   - Did this binary access PII files?
 *   - Did it make outbound network connections?
 *   - Did it spawn subprocesses?
 *   - Did it read sensitive env vars?
 *
 * Richer DSL (regex, address ranges, custom functions) lands in a
 * follow-up PR (TODO.complete/20 filter DSL).
 */

enum Severity {
	SEV_INFO = 0,
	SEV_MEDIUM = 1,
	SEV_HIGH = 2,
	SEV_CRITICAL = 3
};

struct Rule {
	char id[64];

	char description[256];

	/* Predicate fields (NULL/0 = wildcard) */
	const char *func_prefix;

	const char *func_exact;

	const char *path_contains;

	const char *env_pattern;

	enum Severity severity;
};

struct Policy {
	char name[64];

	struct Rule *rules;

	size_t rules_count;
};

/*
 * Load a policy from a JSON document of shape:
 *   {
 *     "name": "baseline",
 *     "rules": [
 *       { "id": "...", "description": "...",
 *         "match": { "func_exact": "system", ... },
 *         "severity": "high" }
 *     ]
 *   }
 *
 * Returns 0 on success, -1 on parse error (logs to stderr).
 * The caller owns the returned Policy and must call policy_free().
 */
int policy_load_from_json(JSON_Object *root, struct Policy *out);

void policy_free(struct Policy *p);

/*
 * Convenience: load a policy from a file path.
 */
int policy_load_from_file(const char *path, struct Policy *out);

/*
 * Returns the severity as a string for JSON output.
 */
const char *severity_str(enum Severity s);

/*
 * Predicate evaluation. The entry is the log entry's "message"
 * object (the part that varies by action). For log_params
 * entries, "func" is the intercepted function name and other
 * fields are the parsed arguments (path, buf, etc.).
 *
 * Predicate semantics: a rule matches if ALL of its non-NULL
 * constraints match (AND). NULL constraints are wildcards.
 *
 * env_pattern glob shapes:
 *   *_TOKEN, *_KEY, *_PASSWORD  (suffix)
 *   LD_*, IFS                   (prefix)
 *   (anything else is exact)
 *
 * Returns 1 if the rule matches, 0 otherwise (including when
 * either argument is NULL).
 */
int policy_rule_matches(const struct Rule *rule, JSON_Object *msg);

#endif /* RETRACE_AUDIT_POLICY_H_ */
