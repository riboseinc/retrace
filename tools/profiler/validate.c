/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Profile contract validation (TODO.trace-profile/06). See
 * validate.h. Checks mirror share/profile-schema.json plus the
 * cross-field rules a schema cannot express (risk present iff
 * the kernel layer was captured).
 */

#include "validate.h"
#include "parson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

struct vctx {
	char *err;
	size_t errsz;
	size_t used;
	int violations;
};

static void violate(struct vctx *c, const char *fmt, ...)
{
	va_list ap;
	int n;

	c->violations++;
	if (c->used + 2 >= c->errsz)
		return;
	if (c->used > 0) {
		c->err[c->used++] = '\n';
		c->err[c->used] = '\0';
	}
	va_start(ap, fmt);
	n = vsnprintf(c->err + c->used, c->errsz - c->used, fmt, ap);
	va_end(ap);
	if (n > 0)
		c->used += (size_t)n;
	if (c->used > c->errsz - 1)
		c->used = c->errsz - 1;
}

static void check_name_count_array(struct vctx *c, JSON_Object *root,
				    const char *key)
{
	JSON_Array *arr = json_object_get_array(root, key);
	size_t i;

	if (arr == NULL) {
		violate(c, "profile.%s: array required", key);
		return;
	}
	for (i = 0; i < json_array_get_count(arr); i++) {
		JSON_Object *o = json_array_get_object(arr, i);
		const char *name;

		if (o == NULL) {
			violate(c, "profile.%s[%zu]: object required",
				key, i);
			continue;
		}
		name = json_object_get_string(o, "name");
		if (name == NULL || name[0] == '\0')
			violate(c, "profile.%s[%zu].name: non-empty string required",
				key, i);
		if (!json_object_has_value(o, "count") ||
		    json_object_get_number(o, "count") < 1)
			violate(c, "profile.%s[%zu].count: number >= 1 required",
				key, i);
	}
}

static int str_in(const char *s, const char *const *set)
{
	size_t i;

	for (i = 0; set[i] != NULL; i++)
		if (strcmp(s, set[i]) == 0)
			return 1;
	return 0;
}

int prof_validate_file(const char *path, char *err, size_t errsz)
{
	static const char *const classes[] = {
		"read", "write", "probe", "none", NULL
	};
	static const char *const layers[] = {
		"captured", "ABSENT", NULL
	};
	static const char *const verdicts[] = {
		"clean", "SUBLIBC_ACCESS_FOUND", NULL
	};
	JSON_Value *v;
	JSON_Object *root;
	JSON_Object *prof;
	JSON_Object *cov;
	struct vctx c;
	int kernel_captured;

	if (err != NULL && errsz > 0)
		err[0] = '\0';
	c.err = err;
	c.errsz = errsz;
	c.used = 0;
	c.violations = 0;

	v = json_parse_file(path);
	if (v == NULL) {
		violate(&c, "not parseable JSON");
		return c.violations ? -1 : 0;
	}
	root = json_value_get_object(v);
	if (root == NULL) {
		violate(&c, "root: object required");
		json_value_free(v);
		return -1;
	}

	prof = json_object_get_object(root, "profile");
	if (prof == NULL) {
		violate(&c, "profile: object required");
	} else {
		JSON_Array *acc;
		size_t i;

		if (!json_object_has_value(prof, "entries") ||
		    json_object_get_number(prof, "entries") < 0)
			violate(&c, "profile.entries: number >= 0 required");
		check_name_count_array(&c, prof, "functions");
		check_name_count_array(&c, prof, "env");
		check_name_count_array(&c, prof, "net");

		acc = json_object_get_array(prof, "accesses");
		if (acc == NULL) {
			violate(&c, "profile.accesses: array required");
		} else {
			for (i = 0; i < json_array_get_count(acc); i++) {
				JSON_Object *o = json_array_get_object(acc, i);
				const char *path_str;
				const char *cls;

				if (o == NULL) {
					violate(&c, "profile.accesses[%zu]: object required", i);
					continue;
				}
				path_str = json_object_get_string(o, "path");
				if (path_str == NULL || path_str[0] == '\0')
					violate(&c, "profile.accesses[%zu].path: non-empty string required", i);
				cls = json_object_get_string(o, "class");
				if (cls == NULL || !str_in(cls, classes))
					violate(&c, "profile.accesses[%zu].class: read|write|probe|none required", i);
				if (!json_object_has_value(o, "hits") ||
				    json_object_get_number(o, "hits") < 1)
					violate(&c, "profile.accesses[%zu].hits: number >= 1 required", i);
			}
		}
	}

	cov = json_object_get_object(root, "coverage");
	kernel_captured = 0;
	if (cov == NULL) {
		violate(&c, "coverage: object required");
	} else {
		const char *l;

		l = json_object_get_string(cov, "libc_layer");
		if (l == NULL || !str_in(l, layers))
			violate(&c, "coverage.libc_layer: captured|ABSENT required");
		l = json_object_get_string(cov, "kernel_layer");
		if (l == NULL || !str_in(l, layers)) {
			violate(&c, "coverage.kernel_layer: captured|ABSENT required");
		} else if (strcmp(l, "captured") == 0) {
			kernel_captured = 1;
		}
	}

	/* cross-field: the risk section exists iff kernel captured */
	if (json_object_has_value(root, "risk")) {
		JSON_Object *risk = json_object_get_object(root, "risk");
		const char *verdict;

		if (!kernel_captured)
			violate(&c, "risk: present but coverage.kernel_layer is ABSENT");
		if (risk == NULL) {
			violate(&c, "risk: object required");
		} else {
			int fi;

			verdict = json_object_get_string(risk, "verdict");
			if (verdict == NULL || !str_in(verdict, verdicts))
				violate(&c, "risk.verdict: clean|SUBLIBC_ACCESS_FOUND required");
			for (fi = 0; fi < 3; fi++) {
				static const char *const nums[] = {
					"agreed", "libc_only", "kernel_only"
				};

				if (!json_object_has_value(risk, nums[fi]) ||
				    json_object_get_number(risk, nums[fi]) < 0)
					violate(&c, "risk.%s: number >= 0 required",
						nums[fi]);
			}
		}
	} else if (kernel_captured) {
		violate(&c, "risk: required when coverage.kernel_layer is captured");
	}

	if (json_object_has_value(root, "static_capability")) {
		JSON_Object *cap = json_object_get_object(root, "static_capability");

		if (cap == NULL) {
			violate(&c, "static_capability: object required");
		} else {
			if (!json_object_has_value(cap, "raw_syscall_gadgets") ||
			    json_object_get_number(cap, "raw_syscall_gadgets") < 0)
				violate(&c, "static_capability.raw_syscall_gadgets: number >= 0 required");
			if (!json_object_has_value(cap, "ntdll_imports") ||
			    json_object_get_number(cap, "ntdll_imports") < 0)
				violate(&c, "static_capability.ntdll_imports: number >= 0 required");
		}
	}

	json_value_free(v);
	return c.violations;
}
