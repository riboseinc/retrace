/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "scan.h"

#include <stdlib.h>

void audit_findings_init(struct Findings *f)
{
	f->items = NULL;
	f->count = 0;
	f->cap = 0;
}

void audit_findings_free(struct Findings *f)
{
	free(f->items);
	f->items = NULL;
	f->count = 0;
	f->cap = 0;
}

int audit_findings_append(struct Findings *f, const struct Rule *rule,
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

void audit_scan_trace(JSON_Array *trace, const struct Policy *policy,
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

			if (policy_rule_matches(rule, msg))
				audit_findings_append(out, rule, i, msg);
		}
	}
}
