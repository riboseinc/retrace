/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static enum Severity parse_severity(const char *s)
{
	if (s == NULL)
		return SEV_INFO;
	if (strcmp(s, "medium") == 0)
		return SEV_MEDIUM;
	if (strcmp(s, "high") == 0)
		return SEV_HIGH;
	if (strcmp(s, "critical") == 0)
		return SEV_CRITICAL;
	return SEV_INFO;
}

const char *severity_str(enum Severity s)
{
	switch (s) {
	case SEV_MEDIUM:
		return "medium";
	case SEV_HIGH:
		return "high";
	case SEV_CRITICAL:
		return "critical";
	case SEV_INFO:
	default:
		return "info";
	}
}

static char *str_dup(const char *s)
{
	size_t n;
	char *out;

	if (s == NULL)
		return NULL;
	n = strlen(s);
	out = (char *)malloc(n + 1);
	if (out == NULL)
		return NULL;
	memcpy(out, s, n + 1);
	return out;
}

/*
 * Copy a string field into a fixed-size buffer. Truncates if too long;
 * always NUL-terminates.
 */
static void set_fixed(char *dst, size_t dstsz, const char *src)
{
	size_t n;

	if (dst == NULL || dstsz == 0)
		return;
	dst[0] = '\0';
	if (src == NULL)
		return;
	n = strlen(src);
	if (n >= dstsz)
		n = dstsz - 1;
	memcpy(dst, src, n);
	dst[n] = '\0';
}

int policy_load_from_json(JSON_Object *root, struct Policy *out)
{
	JSON_Array *rules;
	const char *name;
	size_t i, n;

	if (root == NULL || out == NULL)
		return -1;

	name = json_object_get_string(root, "name");
	set_fixed(out->name, sizeof(out->name), name ? name : "(unnamed)");

	rules = json_object_get_array(root, "rules");
	if (rules == NULL) {
		out->rules = NULL;
		out->rules_count = 0;
		return 0;
	}

	n = json_array_get_count(rules);
	out->rules = (struct Rule *)calloc(n, sizeof(struct Rule));
	if (out->rules == NULL)
		return -1;
	out->rules_count = n;

	for (i = 0; i < n; i++) {
		JSON_Object *r = json_array_get_object(rules, i);
		JSON_Object *m;
		struct Rule *rule = &out->rules[i];

		set_fixed(rule->id, sizeof(rule->id),
			json_object_get_string(r, "id"));
		set_fixed(rule->description, sizeof(rule->description),
			json_object_get_string(r, "description"));
		rule->severity = parse_severity(
			json_object_get_string(r, "severity"));

		m = json_object_get_object(r, "match");
		if (m != NULL) {
			rule->func_prefix = str_dup(
				json_object_get_string(m, "func_prefix"));
			rule->func_exact = str_dup(
				json_object_get_string(m, "func_exact"));
			rule->path_contains = str_dup(
				json_object_get_string(m, "path_contains"));
			rule->env_pattern = str_dup(
				json_object_get_string(m, "env_pattern"));
		}
	}

	return 0;
}

void policy_free(struct Policy *p)
{
	size_t i;

	if (p == NULL)
		return;
	for (i = 0; i < p->rules_count; i++) {
		free((void *)p->rules[i].func_prefix);
		free((void *)p->rules[i].func_exact);
		free((void *)p->rules[i].path_contains);
		free((void *)p->rules[i].env_pattern);
	}
	free(p->rules);
	p->rules = NULL;
	p->rules_count = 0;
}

int policy_load_from_file(const char *path, struct Policy *out)
{
	JSON_Value *v;

	v = json_parse_file(path);
	if (v == NULL) {
		fprintf(stderr, "retrace-audit: cannot parse %s\n", path);
		return -1;
	}
	if (json_value_get_type(v) != JSONObject) {
		fprintf(stderr, "retrace-audit: %s is not a JSON object\n",
			path);
		json_value_free(v);
		return -1;
	}
	int rc = policy_load_from_json(json_value_get_object(v), out);

	json_value_free(v);
	return rc;
}
