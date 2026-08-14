/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_AUDIT_SCAN_H_
#define RETRACE_AUDIT_SCAN_H_

#include "parson.h"
#include "policy.h"

#include <stddef.h>

/*
 * Audit scan engine (TODO.complete/26).
 *
 * Owns the "apply policy to trace" step: walks every
 * (entry, rule) pair and collects matches as Findings. Rule
 * semantics live in policy.c (parsing + matching); this module is
 * the engine that drives them; the formatters in audit.c render
 * the results.
 *
 * A Finding borrows its rule pointer from the Policy and its entry
 * pointer from the trace JSON -- both must outlive the Findings.
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

void audit_findings_init(struct Findings *f);

/* Frees the item array (not the borrowed rule/entry pointers). */
void audit_findings_free(struct Findings *f);

/* Returns 0 on success, -1 on OOM. */
int audit_findings_append(struct Findings *f, const struct Rule *rule,
			  size_t entry_index, JSON_Object *entry);

/*
 * Walks every (entry, rule) pair; each entry whose message matches
 * a rule appends a Finding. Findings appear in trace order; within
 * one entry, in policy-rule order. Entries without a "message"
 * object are skipped.
 */
void audit_scan_trace(JSON_Array *trace, const struct Policy *policy,
		      struct Findings *out);

#endif /* RETRACE_AUDIT_SCAN_H_ */
