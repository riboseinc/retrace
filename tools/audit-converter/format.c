/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "format.h"

#include <string.h>

const char *audit_sarif_level(enum Severity s)
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

JSON_Value *audit_format_default(const struct Policy *policy,
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
		json_array_get_wrapping_value(summary));
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

JSON_Value *audit_format_sarif(const struct Policy *policy,
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
		json_object_set_string(r, "level",
			audit_sarif_level(f->rule->severity));

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
