/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "aggregate.h"
#include "match.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void prof_names_init(struct ProfNames *n)
{
	n->names = NULL;
	n->counts = NULL;
	n->count = 0;
	n->cap = 0;
}

void prof_names_add(struct ProfNames *n, const char *name)
{
	size_t lo = 0, hi = n->count;

	if (name == NULL || name[0] == '\0')
		return;
	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		int r = strcmp(n->names[mid], name);

		if (r == 0) {
			n->counts[mid]++;
			return;
		}
		if (r < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	/* Insert at lo; names kept sorted by insertion + final sort. */
	if (n->count == n->cap) {
		size_t newcap = (n->cap == 0) ? 64 : n->cap * 2;
		char **gn = (char **)realloc(n->names,
			newcap * sizeof(char *));
		size_t *gc = (size_t *)realloc(n->counts,
			newcap * sizeof(size_t));

		if (gn == NULL || gc == NULL)
			return;
		n->names = gn;
		n->counts = gc;
		n->cap = newcap;
	}
	{
		char *copy = (char *)malloc(strlen(name) + 1);

		if (copy == NULL)
			return;
		strcpy(copy, name);

		/* Insert at the bsearch position so the array stays
		 * sorted; appending would hide earlier duplicates
		 * from later lookups.
		 */
		memmove(&n->names[lo + 1], &n->names[lo],
			(n->count - lo) * sizeof(*n->names));
		memmove(&n->counts[lo + 1], &n->counts[lo],
			(n->count - lo) * sizeof(*n->counts));
		n->names[lo] = copy;
		n->counts[lo] = 1;
		n->count++;
	}
}

size_t prof_names_get(const struct ProfNames *n, const char *name)
{
	size_t lo = 0, hi = n->count;

	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		int r = strcmp(n->names[mid], name);

		if (r == 0)
			return n->counts[mid];
		if (r < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	return 0;
}

void prof_names_free(struct ProfNames *n)
{
	size_t i;

	for (i = 0; i < n->count; i++)
		free(n->names[i]);
	free(n->names);
	free(n->counts);
	prof_names_init(n);
}

static void names_finish(struct ProfNames *n)
{
	size_t i;

	/* Insertion-order with the bsearch-add above only stays
	 * sorted if adds come in order; sort once and rebuild
	 * counts is wrong, so sort indices together.
	 */
	if (n->count < 2)
		return;
	/* Simple paired insertion sort: count is small per name
	 * set and this preserves name/count pairing.
	 */
	for (i = 1; i < n->count; i++) {
		char *nm = n->names[i];
		size_t ct = n->counts[i];
		size_t j = i;

		while (j > 0 && strcmp(n->names[j - 1], nm) > 0) {
			n->names[j] = n->names[j - 1];
			n->counts[j] = n->counts[j - 1];
			j--;
		}
		n->names[j] = nm;
		n->counts[j] = ct;
	}
}

static void prof_timing_add(struct Profile *p, const char *func,
			    double us);

static int timing_dcmp(const void *a, const void *b)
{
	double da = *(const double *)a;
	double db = *(const double *)b;

	return da < db ? -1 : da > db ? 1 : 0;
}

double prof_timing_p99(const struct ProfTiming *t)
{
	if (t->sample_cnt == 0)
		return 0;
	{
		double *sorted = (double *)malloc(
			t->sample_cnt * sizeof(*sorted));

		if (sorted == NULL)
			return t->max_us;
		memcpy(sorted, t->samples,
			t->sample_cnt * sizeof(*sorted));
		qsort(sorted, t->sample_cnt, sizeof(*sorted),
			timing_dcmp);
		{
			size_t idx = t->sample_cnt >= 100 ?
				t->sample_cnt - t->sample_cnt / 100 :
				t->sample_cnt - 1;
			double v = sorted[idx];

			free(sorted);
			return v;
		}
	}
}

static void prof_timing_add(struct Profile *p, const char *func,
			    double us)
{
	struct ProfTiming *t = NULL;
	size_t lo = 0, hi = p->timings.count;

	/* sorted insert position (bsearch) */
	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		int r = strcmp(p->timings.items[mid].func, func);

		if (r == 0) {
			t = &p->timings.items[mid];
			break;
		}
		if (r < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	if (t == NULL) {
		if (p->timings.count == p->timings.cap) {
			size_t newcap = p->timings.cap == 0 ?
				16 : p->timings.cap * 2;
			struct ProfTiming *grown =
				(struct ProfTiming *)realloc(
					p->timings.items,
					newcap * sizeof(*grown));

			if (grown == NULL)
				return;
			p->timings.items = grown;
			p->timings.cap = newcap;
		}
		{
			struct ProfTiming nt;
			size_t tail = p->timings.count - lo;

			memset(&nt, 0, sizeof(nt));
			nt.func = (char *)malloc(strlen(func) + 1);
			if (nt.func == NULL)
				return;
			strcpy(nt.func, func);
			memmove(&p->timings.items[lo + 1],
				&p->timings.items[lo],
				tail * sizeof(*p->timings.items));
			p->timings.items[lo] = nt;
			p->timings.count++;
			t = &p->timings.items[lo];
		}
	}
	t->calls++;
	t->total_us += us;
	if (us > t->max_us)
		t->max_us = us;
	if (t->sample_cnt < PROF_TIMING_SAMPLES_MAX) {
		if (t->sample_cnt == t->sample_cap) {
			size_t newcap = t->sample_cap == 0 ?
				16 : t->sample_cap * 2;
			double *grown = (double *)realloc(t->samples,
				newcap * sizeof(*grown));

			if (grown == NULL)
				return;
			t->samples = grown;
			t->sample_cap = newcap;
		}
		t->samples[t->sample_cnt++] = us;
	}
}

void prof_init(struct Profile *p)
{
	prof_names_init(&p->functions);
	prof_names_init(&p->env);
	prof_names_init(&p->net);
	memset(&p->timings, 0, sizeof(p->timings));
	p->accesses.items = NULL;
	p->accesses.count = 0;
	p->accesses.cap = 0;
	p->entries = 0;
	p->no_pid = 0;
}

struct ProfAccess *prof_access_get(struct Profile *p, const char *path)
{
	size_t lo = 0, hi = p->accesses.count;

	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		int r = strcmp(p->accesses.items[mid].path, path);

		if (r == 0)
			return &p->accesses.items[mid];
		if (r < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	return NULL;
}

static void access_add(struct Profile *p, const char *path,
		       enum CorrClass cls)
{
	struct ProfAccess *a = NULL;

	/* Sorted insert (bsearch position). */
	size_t lo = 0, hi = p->accesses.count;

	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		int r = strcmp(p->accesses.items[mid].path, path);

		if (r == 0) {
			/* match is at mid, NOT lo (lo only converges
			 * on the insert position for misses)
			 */
			a = &p->accesses.items[mid];
			break;
		}
		if (r < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	if (a == NULL) {
		size_t tail = p->accesses.count - lo;

		if (p->accesses.count == p->accesses.cap) {
			size_t newcap = (p->accesses.cap == 0) ?
				64 : p->accesses.cap * 2;
			struct ProfAccess *grown =
				(struct ProfAccess *)realloc(
					p->accesses.items,
					newcap * sizeof(*grown));

			if (grown == NULL)
				return;
			p->accesses.items = grown;
			p->accesses.cap = newcap;
		}
		memmove(&p->accesses.items[lo + 1],
			&p->accesses.items[lo],
			tail * sizeof(*p->accesses.items));
		a = &p->accesses.items[lo];
		a->path = (char *)malloc(strlen(path) + 1);
		if (a->path == NULL)
			return;
		strcpy(a->path, path);
		a->class_write = 0;
		a->class_read = 0;
		a->class_probe = 0;
		a->hits = 0;
		p->accesses.count++;
	}
	a->hits++;
	if (cls == CORR_CLS_WRITE)
		a->class_write = 1;
	else if (cls == CORR_CLS_READ)
		a->class_read = 1;
	else if (cls == CORR_CLS_PROBE)
		a->class_probe = 1;
}

/*
 * Env var names. The live logger keeps the actual name in the
 * deref array "*name" (the plain "name" value is the pointer
 * string); the golden parity format stores it directly.
 */
static void collect_env(const JSON_Object *msg, struct Profile *p)
{
	JSON_Array *deref = json_object_get_array(msg, "*name");
	const char *name;

	if (deref != NULL) {
		if (json_array_get_count(deref) > 0) {
			name = json_array_get_string(deref, 0);
			if (name != NULL && name[0] != '\0')
				prof_names_add(&p->env, name);
		}
		return;
	}
	name = json_object_get_string(msg, "name");
	if (name != NULL && name[0] != '\0')
		prof_names_add(&p->env, name);
}

static void collect_net(const JSON_Object *msg, struct Profile *p)
{
	const char *host = json_object_get_string(msg, "host");
	const char *port = json_object_get_string(msg, "port");
	const char *addr = json_object_get_string(msg, "addr");

	if (addr != NULL && addr[0] != '\0') {
		prof_names_add(&p->net, addr);
	} else if (host != NULL && host[0] != '\0') {
		char buf[256];

		if (port != NULL && port[0] != '\0')
			snprintf(buf, sizeof(buf), "%s:%s", host, port);
		else
			snprintf(buf, sizeof(buf), "%s", host);
		prof_names_add(&p->net, buf);
	}
}

/*
 * Depth-first string walk (same shape as the correlator's):
 * classify the entry by func, add every path-like string. Pointer
 * params appear as "0x..." strings (skipped); their deref arrays
 * carry the real values.
 */
static void collect_msg(const JSON_Value *v, struct Profile *p,
			const char *func, const char *detail)
{
	size_t i, n;

	if (v == NULL)
		return;
	switch (json_value_get_type(v)) {
	case JSONString:
		if (corr_is_path_like(json_value_get_string(v))) {
			char norm[CORR_PATH_MAX];

			if (corr_normalize(json_value_get_string(v), norm,
					sizeof(norm)) > 0)
				access_add(p, norm,
					corr_classify(func, detail));
		}
		break;
	case JSONObject:
		n = json_object_get_count(json_value_get_object(v));
		for (i = 0; i < n; i++)
			collect_msg(json_object_get_value_at(
					    json_value_get_object(v), i),
				p, func, detail);
		break;
	case JSONArray:
		n = json_array_get_count(json_value_get_array(v));
		for (i = 0; i < n; i++)
			collect_msg(json_array_get_value(
					    json_value_get_array(v), i),
				p, func, detail);
		break;
	default:
		break;
	}
}

void prof_add_entry(JSON_Object *entry, struct Profile *p)
{
	JSON_Object *msg;
	const char *func;
	const char *detail;

	if (entry == NULL)
		return;
	p->entries++;
	if (json_object_get_number(entry, "pid") == 0)
		p->no_pid++;

	msg = json_object_get_object(entry, "message");
	if (msg == NULL)
		return;

	/*
	 * Banner entries (engine progress text) are not calls; walking
	 * them would pollute the profile with the tracer's own config
	 * path. Return summaries (call timing) re-state func with no
	 * new params; counting them would double every call. Same
	 * entry taxonomy as the CLI pretty-printer.
	 */
	if (json_object_get_string(msg, "text") != NULL)
		return;
	if (json_object_has_value(msg, "call_duration_us")) {
		/* timings harvest (TODO.trace-profile/21): the return
		 * summary re-states func + duration
		 */
		const char *tf = json_object_get_string(msg, "func");

		if (tf != NULL && tf[0] != '\0')
			prof_timing_add(p, tf,
				json_object_get_number(msg,
					"call_duration_us"));
		return;
	}

	func = json_object_get_string(msg, "func");
	detail = json_object_get_string(msg, "detail");
	if (func != NULL)
		prof_names_add(&p->functions, func);

	collect_env(msg, p);
	collect_net(msg, p);
	collect_msg(json_object_get_wrapping_value(msg), p, func, detail);
}

void prof_finish(struct Profile *p)
{
	names_finish(&p->functions);
	names_finish(&p->env);
	names_finish(&p->net);
	/* accesses are sorted by construction (sorted insert). */
}

void prof_free(struct Profile *p)
{
	size_t i;

	prof_names_free(&p->functions);
	prof_names_free(&p->env);
	prof_names_free(&p->net);
	for (i = 0; i < p->timings.count; i++) {
		free(p->timings.items[i].func);
		free(p->timings.items[i].samples);
	}
	free(p->timings.items);
	memset(&p->timings, 0, sizeof(p->timings));
	for (i = 0; i < p->accesses.count; i++)
		free(p->accesses.items[i].path);
	free(p->accesses.items);
	p->accesses.items = NULL;
	p->accesses.count = 0;
	p->accesses.cap = 0;
}

static void names_to_json(const struct ProfNames *n, JSON_Object *root,
			  const char *key)
{
	JSON_Value *arr = json_value_init_array();
	size_t i;

	for (i = 0; i < n->count; i++) {
		JSON_Value *o = json_value_init_object();

		json_object_set_string(json_value_get_object(o), "name",
			n->names[i]);
		json_object_set_number(json_value_get_object(o), "count",
			(double)n->counts[i]);
		json_array_append_value(json_value_get_array(arr), o);
	}
	json_object_set_value(root, key, arr);
}

static const char *access_class(const struct ProfAccess *a)
{
	if (a->class_write)
		return "write";
	if (a->class_read)
		return "read";
	if (a->class_probe)
		return "probe";
	return "none";
}

JSON_Value *prof_to_json(const struct Profile *p)
{
	JSON_Value *v = json_value_init_object();
	JSON_Object *root = json_value_get_object(v);
	JSON_Value *arr;
	size_t i;

	json_object_set_number(root, "entries", (double)p->entries);
	names_to_json(&p->functions, root, "functions");
	names_to_json(&p->env, root, "env");
	names_to_json(&p->net, root, "net");

	arr = json_value_init_array();
	for (i = 0; i < p->accesses.count; i++) {
		const struct ProfAccess *a = &p->accesses.items[i];
		JSON_Value *o = json_value_init_object();

		json_object_set_string(json_value_get_object(o), "path",
			a->path);
		json_object_set_string(json_value_get_object(o), "class",
			access_class(a));
		json_object_set_number(json_value_get_object(o), "hits",
			(double)a->hits);
		json_array_append_value(json_value_get_array(arr), o);
	}
	json_object_set_value(root, "accesses", arr);

	/* timings top-N by total (TODO.trace-profile/21) */
	if (p->timings.count > 0) {
		JSON_Value *tv = json_value_init_array();
		size_t top = p->timings.count < 20 ?
			p->timings.count : 20;
		size_t *order = (size_t *)malloc(
			p->timings.count * sizeof(*order));
		size_t i, j;

		if (order != NULL) {
			for (i = 0; i < p->timings.count; i++)
				order[i] = i;
			/* selection sort by total desc over the top
			 * slice only (count is small; no qsort cmp
			 * closure available)
			 */
			for (i = 0; i < top; i++) {
				size_t best = i;

				for (j = i + 1; j < p->timings.count;
					j++) {
					if (p->timings.items[order[j]]
					    .total_us >
					    p->timings.items[order[best]]
					    .total_us)
						best = j;
				}
				if (best != i) {
					size_t tmp = order[i];

					order[i] = order[best];
					order[best] = tmp;
				}
			}
			for (i = 0; i < top; i++) {
				const struct ProfTiming *t =
					&p->timings.items[order[i]];
				JSON_Value *o = json_value_init_object();

				json_object_set_string(
					json_value_get_object(o),
					"func", t->func);
				json_object_set_number(
					json_value_get_object(o),
					"calls", (double)t->calls);
				json_object_set_number(
					json_value_get_object(o),
					"total_us", t->total_us);
				json_object_set_number(
					json_value_get_object(o),
					"max_us", t->max_us);
				json_object_set_number(
					json_value_get_object(o),
					"p99_us", prof_timing_p99(t));
				json_array_append_value(
					json_value_get_array(tv), o);
			}
			free(order);
			json_object_set_value(root, "timings", tv);
		} else {
			json_value_free(tv);
		}
	}
	return v;
}

static size_t names_index(const struct ProfNames *n, const char *name)
{
	size_t lo = 0, hi = n->count;

	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		int r = strcmp(n->names[mid], name);

		if (r == 0)
			return mid;
		if (r < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	return n->count; /* miss: never a valid index */
}

static void names_from_json(struct ProfNames *n, const JSON_Object *root,
			    const char *key)
{
	JSON_Array *arr = json_object_get_array(root, key);
	size_t i;

	for (i = 0; arr != NULL && i < json_array_get_count(arr); i++) {
		JSON_Object *o = json_array_get_object(arr, i);
		const char *name = json_object_get_string(o, "name");
		double count = json_object_get_number(o, "count");
		size_t idx;

		if (name == NULL || name[0] == '\0')
			continue;
		prof_names_add(n, name);
		idx = names_index(n, name);
		if (idx != n->count && count > 1)
			n->counts[idx] = (size_t)count;
	}
}

static enum CorrClass class_from_str(const char *s)
{
	if (s == NULL)
		return CORR_CLS_NONE;
	if (strcmp(s, "write") == 0)
		return CORR_CLS_WRITE;
	if (strcmp(s, "read") == 0)
		return CORR_CLS_READ;
	if (strcmp(s, "probe") == 0)
		return CORR_CLS_PROBE;
	return CORR_CLS_NONE;
}

/*
 * prof_to_json's inverse (SSOT: the shape and its reader live in
 * the same module). Rebuilds a Profile from the "profile" object
 * of a profile doc -- the jail subcommand re-emits a config from
 * an existing profile without re-aggregating the trace.
 */
int prof_from_json(const JSON_Object *root, struct Profile *p)
{
	JSON_Array *arr;
	size_t i;

	prof_init(p);
	if (root == NULL)
		return -1;

	p->entries = (size_t)json_object_get_number(root, "entries");
	names_from_json(&p->functions, root, "functions");
	names_from_json(&p->env, root, "env");
	/*
	 * Timings round-trip (TODO.trace-profile/30): the profile
	 * doc carries the AGGREGATES (calls/total/max/p99 -- raw
	 * samples are intentionally not serialized); restore them
	 * so offline consumers (export) see the same numbers.
	 */
	{
		JSON_Array *ta = json_object_get_array(root, "timings");

		if (ta != NULL) {
			size_t ti;

			for (ti = 0; ti < json_array_get_count(ta);
			     ti++) {
				JSON_Object *to =
					json_array_get_object(ta, ti);
				const char *func = to != NULL ?
					json_object_get_string(to,
						"func") : NULL;
				struct ProfTiming *t;

				if (func == NULL)
					continue;
				prof_timing_add(p, func,
					json_object_get_number(to,
						"max_us"));
				/* the add above created/located it;
				 * find it again via the same bsearch
				 */
				{
					size_t lo2 = 0, hi2 =
						p->timings.count;

					t = NULL;
					while (lo2 < hi2) {
						size_t mid = lo2 +
							(hi2 - lo2) / 2;
						int r = strcmp(
							p->timings.items[mid].func,
							func);

						if (r == 0) {
							t = &p->timings.items[mid];
							break;
						}
						if (r < 0)
							lo2 = mid + 1;
						else
							hi2 = mid;
					}
				}
				if (t != NULL) {
					t->calls = (size_t)
						json_object_get_number(to,
							"calls");
					t->total_us =
						json_object_get_number(to,
							"total_us");
				}
			}
		}
	}
	names_from_json(&p->net, root, "net");

	arr = json_object_get_array(root, "accesses");
	for (i = 0; arr != NULL && i < json_array_get_count(arr); i++) {
		JSON_Object *o = json_array_get_object(arr, i);
		const char *path = json_object_get_string(o, "path");
		double hits = json_object_get_number(o, "hits");

		if (path == NULL || path[0] == '\0')
			continue;
		access_add(p, path,
			class_from_str(json_object_get_string(o,
							     "class")));
		{
			struct ProfAccess *a = prof_access_get(p, path);

			if (a != NULL && hits > 0)
				a->hits = (size_t)hits;
		}
	}
	prof_finish(p);
	return 0;
}
