/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Profile drift computation (TODO.trace-profile/02). Both
 * profiles arrive aggregated (aggregate.c) and finished; this
 * module walks the two sorted path arrays and the two sorted
 * name arrays once each -- an O(n+m) merge, not a search per
 * element.
 */

#include "diff.h"
#include "match.h"

#include <stdlib.h>
#include <string.h>

static const char *cls_str(int cls)
{
	if (cls < 0)
		return "-";
	return corr_class_str((enum CorrClass)cls);
}

void prof_diff_init(struct ProfDiff *d)
{
	memset(d, 0, sizeof(*d));
}

static void change_add(struct ProfDiff *d, const char *path,
		       int from, int to, size_t hits_from,
		       size_t hits_to)
{
	struct ProfPathChange *c;

	if (d->count == d->cap) {
		size_t newcap = (d->cap == 0) ? 16 : d->cap * 2;

		d->changes = (struct ProfPathChange *)realloc(d->changes,
			newcap * sizeof(*d->changes));
		if (d->changes == NULL)
			return;
		d->cap = newcap;
	}
	c = &d->changes[d->count++];
	c->path = (char *)malloc(strlen(path) + 1);
	if (c->path == NULL) {
		d->count--;
		return;
	}
	strcpy(c->path, path);
	c->class_from = from;
	c->class_to = to;
	c->hits_from = hits_from;
	c->hits_to = hits_to;
}

static int path_class(const struct ProfAccess *a)
{
	/* the recorded class precedence matches access_class() */
	if (a->class_write)
		return CORR_CLS_WRITE;
	if (a->class_read)
		return CORR_CLS_READ;
	if (a->class_probe)
		return CORR_CLS_PROBE;
	return CORR_CLS_NONE;
}

static void new_function_add(struct ProfDiff *d, const char *name)
{
	if (d->new_functions_cnt == d->new_functions_cap) {
		size_t newcap = (d->new_functions_cap == 0) ?
			16 : d->new_functions_cap * 2;

		d->new_functions = (char **)realloc(d->new_functions,
			newcap * sizeof(char *));
		if (d->new_functions == NULL)
			return;
		d->new_functions_cap = newcap;
	}
	d->new_functions[d->new_functions_cnt] =
		(char *)malloc(strlen(name) + 1);
	if (d->new_functions[d->new_functions_cnt] == NULL)
		return;
	strcpy(d->new_functions[d->new_functions_cnt], name);
	d->new_functions_cnt++;
}

int prof_diff_compute(const struct Profile *baseline,
		      const struct Profile *candidate,
		      struct ProfDiff *d)
{
	size_t i = 0, j = 0;
	size_t f = 0, g = 0;
	int drift = 0;

	/* merge the sorted accesses arrays */
	while (i < baseline->accesses.count ||
	       j < candidate->accesses.count) {
		const struct ProfAccess *a =
			(i < baseline->accesses.count) ?
			&baseline->accesses.items[i] : NULL;
		const struct ProfAccess *b =
			(j < candidate->accesses.count) ?
			&candidate->accesses.items[j] : NULL;
		int cmp;

		if (a == NULL)
			cmp = 1;
		else if (b == NULL)
			cmp = -1;
		else
			cmp = strcmp(a->path, b->path);

		if (cmp < 0) {
			change_add(d, a->path, path_class(a), -1,
				a->hits, 0);
			drift = 1;
			i++;
		} else if (cmp > 0) {
			change_add(d, b->path, -1, path_class(b), 0,
				b->hits);
			drift = 1;
			j++;
		} else {
			int from = path_class(a);
			int to = path_class(b);

			if (from != to) {
				change_add(d, a->path, from, to,
					a->hits, b->hits);
				drift = 1;
			}
			i++;
			j++;
		}
	}

	/* merge the sorted function-name arrays: candidate-only */
	while (f < baseline->functions.count ||
	       g < candidate->functions.count) {
		const char *fa = (f < baseline->functions.count) ?
			baseline->functions.names[f] : NULL;
		const char *fc = (g < candidate->functions.count) ?
			candidate->functions.names[g] : NULL;
		int cmp;

		if (fa == NULL)
			cmp = 1;
		else if (fc == NULL)
			cmp = -1;
		else
			cmp = strcmp(fa, fc);

		if (cmp < 0) {
			f++;
		} else if (cmp > 0) {
			new_function_add(d, fc);
			drift = 1;
			g++;
		} else {
			f++;
			g++;
		}
	}

	return drift;
}

static void change_to_json(JSON_Array *arr,
			   const struct ProfPathChange *c)
{
	JSON_Value *o = json_value_init_object();
	JSON_Object *obj = json_value_get_object(o);

	json_object_set_string(obj, "path", c->path);
	if (c->class_from < 0) {
		json_object_set_string(obj, "change", "added");
	} else if (c->class_to < 0) {
		json_object_set_string(obj, "change", "removed");
	} else {
		json_object_set_string(obj, "change", "class");
		json_object_set_string(obj, "from", cls_str(c->class_from));
		json_object_set_string(obj, "to", cls_str(c->class_to));
	}
	json_object_set_number(obj, "hits_from", (double)c->hits_from);
	json_object_set_number(obj, "hits_to", (double)c->hits_to);
	json_array_append_value(arr, o);
}

JSON_Value *prof_diff_to_json(const struct ProfDiff *d)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);
	JSON_Value *arr = json_value_init_array();
	JSON_Value *farr = json_value_init_array();
	size_t i;
	int any = 0;

	for (i = 0; i < d->count; i++) {
		change_to_json(json_value_get_array(arr), &d->changes[i]);
		any = 1;
	}
	for (i = 0; i < d->new_functions_cnt; i++) {
		JSON_Value *o = json_value_init_object();

		json_object_set_string(json_value_get_object(o),
			"name", d->new_functions[i]);
		json_array_append_value(json_value_get_array(farr), o);
		any = 1;
	}

	json_object_set_value(root, "path_changes", arr);
	json_object_set_value(root, "new_functions", farr);
	json_object_set_boolean(root, "drift", any);
	return root_val;
}

void prof_diff_free(struct ProfDiff *d)
{
	size_t i;

	for (i = 0; i < d->count; i++)
		free(d->changes[i].path);
	free(d->changes);
	for (i = 0; i < d->new_functions_cnt; i++)
		free(d->new_functions[i]);
	free(d->new_functions);
	memset(d, 0, sizeof(*d));
}
