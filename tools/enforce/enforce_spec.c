/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include <stdio.h>
#include <string.h>

#include "enforce_spec.h"
#include "parson.h"

static unsigned int access_mask(JSON_Object *o)
{
	JSON_Array *a = json_object_get_array(o, "access");
	unsigned int m = 0;
	size_t i;

	if (a == NULL)
		return 0;
	for (i = 0; i < json_array_get_count(a); i++) {
		const char *s = json_array_get_string(a, i);

		if (s == NULL)
			continue;
		if (strcmp(s, "rd") == 0)
			m |= ENF_READ;
		else if (strcmp(s, "wr") == 0)
			m |= ENF_WRITE;
		else if (strcmp(s, "x") == 0)
			m |= ENF_EXECUTE;
	}
	return m;
}

static int parse_rules(struct enforce_spec *spec, JSON_Object *root)
{
	JSON_Object *ll = json_object_get_object(root, "landlock");
	JSON_Array *rules;
	size_t i;

	if (ll == NULL)
		return 0;
	rules = json_object_get_array(ll, "rules");
	if (rules == NULL)
		return 0;
	for (i = 0; i < json_array_get_count(rules); i++) {
		JSON_Object *r = json_array_get_object(rules, i);
		const char *p;
		unsigned int m;

		if (r == NULL)
			continue;
		p = json_object_get_string(r, "path");
		m = access_mask(r);
		if (p == NULL || m == 0 ||
		    spec->rules_n >= ENFORCE_RULES_MAX)
			continue;
		snprintf(spec->rules[spec->rules_n].path,
			sizeof(spec->rules[spec->rules_n].path), "%s", p);
		spec->rules[spec->rules_n].access = m;
		spec->rules_n++;
	}
	return 0;
}

static int parse_deny(struct enforce_spec *spec, JSON_Object *root)
{
	JSON_Object *sc = json_object_get_object(root, "seccomp");
	JSON_Array *deny;
	size_t i;

	if (sc == NULL)
		return 0;
	deny = json_object_get_array(sc, "deny");
	if (deny == NULL)
		return 0;
	for (i = 0; i < json_array_get_count(deny); i++) {
		const char *s = json_array_get_string(deny, i);

		if (s == NULL || spec->deny_n >= ENFORCE_SYSCALLS_MAX)
			continue;
		snprintf(spec->deny[spec->deny_n].name,
			sizeof(spec->deny[spec->deny_n].name), "%s", s);
		spec->deny_n++;
	}
	return 0;
}

int enforce_spec_parse(struct enforce_spec *spec, const char *json)
{
	JSON_Value *v = json_parse_string(json);
	JSON_Object *root;

	memset(spec, 0, sizeof(*spec));
	if (v == NULL)
		return -1;
	root = json_value_get_object(v);
	if (root == NULL) {
		json_value_free(v);
		return -1;
	}
	{
		const char *sb = json_object_get_string(root,
			"sandbox_exec");

		if (sb != NULL)
			snprintf(spec->sandbox_exec,
				sizeof(spec->sandbox_exec), "%s", sb);
	}
	spec->no_new_privs =
		json_object_get_boolean(root, "no_new_privs") != 0;
	(void)parse_rules(spec, root);
	(void)parse_deny(spec, root);
	{
		JSON_Object *ac = json_object_get_object(root,
			"appcontainer");

		if (ac != NULL) {
			const char *nm = json_object_get_string(ac, "name");
			JSON_Array *rd = json_object_get_array(ac,
				"read_paths");
			JSON_Array *wr = json_object_get_array(ac,
				"write_paths");
			size_t k, n;

			if (nm != NULL)
				snprintf(spec->ac_name,
					sizeof(spec->ac_name), "%s", nm);
			n = rd != NULL ? json_array_get_count(rd) : 0;
			for (k = 0; k < n &&
			     spec->ac_read_n < ENFORCE_AC_PATHS_MAX; k++) {
				const char *s = json_array_get_string(rd, k);

				if (s != NULL)
					snprintf(
						spec->ac_read[spec->ac_read_n++],
						sizeof(spec->ac_read[0]),
						"%s", s);
			}
			n = wr != NULL ? json_array_get_count(wr) : 0;
			for (k = 0; k < n &&
			     spec->ac_write_n < ENFORCE_AC_PATHS_MAX; k++) {
				const char *s = json_array_get_string(wr, k);

				if (s != NULL)
					snprintf(
						spec->ac_write[spec->ac_write_n++],
						sizeof(spec->ac_write[0]),
						"%s", s);
			}
		}
	}
	json_value_free(v);
	return 0;
}
