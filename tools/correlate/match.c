/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "match.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int corr_is_path_like(const char *s)
{
	size_t n;

	if (s == NULL || s[0] == '\0')
		return 0;
	n = strlen(s);
	if (s[0] == '/' || s[0] == '\\')
		return 1;
	if (n >= 2 && ((s[0] >= 'A' && s[0] <= 'Z') ||
		       (s[0] >= 'a' && s[0] <= 'z')) && s[1] == ':')
		return 1;
	return strchr(s, '/') != NULL || strchr(s, '\\') != NULL;
}

size_t corr_normalize(const char *in, char *out, size_t outsz)
{
	const char *p = in;
	char drive[2] = { 0, 0 };
	size_t prefix = 0;
	size_t n, i;

	if (in == NULL || out == NULL || outsz == 0)
		return 0;
	out[0] = '\0';

	/*
	 * NT prefix forms -> plain path. The '//?/' spelling is the
	 * forward-slash variant libsass builds before flipping the
	 * separators (src/file.cpp read_file/file_exists).
	 */
	if (strncmp(p, "\\??\\", 4) == 0) {
		p += 4;
	} else if (strncmp(p, "//?/", 4) == 0 && p[4] != '\0' &&
		   p[4] != '/') {
		p += 4;
	} else if (strncmp(p, "\\\\?\\", 4) == 0) {
		p += 4;
	} else if (strncmp(p, "\\Device\\HarddiskVolume", 22) == 0) {
		/*
		 * "\Device\HarddiskVolume3\rest" -> guess a DOS drive
		 * letter (volume N -> 'A'+N-1, so volume 3 -> C:, the
		 * first visible volume on a standard install). Both
		 * sides of the join normalize through this function,
		 * so consistency matters more than accuracy.
		 */
		const char *v = p + 22;
		long vol = 0;

		while (*v >= '0' && *v <= '9') {
			vol = vol * 10 + (*v - '0');
			v++;
		}
		if (vol >= 1 && vol <= 26) {
			drive[0] = (char)('A' + vol - 1);
			drive[1] = ':';
			prefix = 2;
		}
		/*
		 * Keep the separator: the '\' after the volume digits
		 * slash-unifies into the '/' of "C:/rest".
		 */
		p = v;
	}

	n = strlen(p);
	if (prefix + n + 1 > outsz)
		return 0;

	memcpy(out, drive, prefix);
	for (i = 0; i < n; i++)
		out[prefix + i] = (p[i] == '\\') ? '/' : p[i];

	/* Drop a trailing slash unless it is the whole path. */
	if (prefix + n > 1 && out[prefix + n - 1] == '/')
		n--;

	out[prefix + n] = '\0';
	if (!corr_is_path_like(out))
		return 0;
	return prefix + n + 1;
}

int corr_pathcmp(const char *a, const char *b)
{
	if (a == NULL || b == NULL)
		return (a == b) ? 0 : (a == NULL ? -1 : 1);
	/* Drive letter case-insensitive; rest case-sensitive. */
	if (((a[0] >= 'A' && a[0] <= 'Z') || (a[0] >= 'a' && a[0] <= 'z'))
	    && a[1] == ':'
	    && ((b[0] >= 'A' && b[0] <= 'Z') || (b[0] >= 'a' && b[0] <= 'z'))
	    && b[1] == ':') {
		char ca = a[0];
		char cb = b[0];
		int r;

		if (ca >= 'a' && ca <= 'z')
			ca = (char)(ca - 'a' + 'A');
		if (cb >= 'a' && cb <= 'z')
			cb = (char)(cb - 'a' + 'A');
		r = (int)(unsigned char)ca - (int)(unsigned char)cb;
		if (r != 0)
			return r;
		a += 2;
		b += 2;
	}
	return strcmp(a, b);
}

void corr_set_init(struct CorrSet *s)
{
	s->items = NULL;
	s->count = 0;
	s->cap = 0;
}

static int cmp_setitem(const void *a, const void *b)
{
	return corr_pathcmp(*(const char *const *)a,
			    *(const char *const *)b);
}

int corr_set_add(struct CorrSet *s, const char *path)
{
	char *copy;
	size_t i;

	if (s == NULL || path == NULL)
		return -1;
	for (i = 0; i < s->count; i++)
		if (corr_pathcmp(s->items[i], path) == 0)
			return 0;
	if (s->count == s->cap) {
		size_t newcap = (s->cap == 0) ? 64 : s->cap * 2;
		char **grown = (char **)realloc(s->items,
			newcap * sizeof(char *));

		if (grown == NULL)
			return -1;
		s->items = grown;
		s->cap = newcap;
	}
	copy = (char *)malloc(strlen(path) + 1);
	if (copy == NULL)
		return -1;
	strcpy(copy, path);
	s->items[s->count++] = copy;
	return 0;
}

void corr_set_finish(struct CorrSet *s)
{
	if (s->items != NULL && s->count > 1)
		qsort(s->items, s->count, sizeof(char *), cmp_setitem);
}

int corr_set_contains(const struct CorrSet *s, const char *path)
{
	size_t lo = 0, hi = s->count;

	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		int r = corr_pathcmp(s->items[mid], path);

		if (r == 0)
			return 1;
		if (r < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	return 0;
}

void corr_set_free(struct CorrSet *s)
{
	size_t i;

	for (i = 0; i < s->count; i++)
		free(s->items[i]);
	free(s->items);
	s->items = NULL;
	s->count = 0;
	s->cap = 0;
}

/* ----- Classification (TODO.windows/03) ----- */

static int name_in(const char *func, const char *const *names)
{
	size_t i;

	if (func == NULL)
		return 0;
	for (i = 0; names[i] != NULL; i++)
		if (strcmp(func, names[i]) == 0)
			return 1;
	return 0;
}

static void fold(char *buf, size_t bufsz, const char *s)
{
	size_t i;

	if (s == NULL) {
		buf[0] = '\0';
		return;
	}
	for (i = 0; s[i] != '\0' && i + 1 < bufsz; i++) {
		char c = s[i];

		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
		buf[i] = c;
	}
	buf[i] = '\0';
}

/* Probe = read-attributes semantics: existence information. */
static const char *const g_probe_funcs[] = {
	"queryopen", "getfileattributesw", "getfileattributesa",
	"ntqueryattributesfile", "stat", "lstat", "fstatat",
	"access", "faccessat", "stat64", NULL
};

/* Write = potential mutation. */
static const char *const g_write_funcs[] = {
	"writefile", "ntwritefile", "deletefilew", "deletefilea",
	"movefilew", "movefileexw", "copyfilew", "rename", "unlink",
	"rmdir", "truncate", "ftruncate", "mkdir", "mkdirat",
	"creat", NULL
};

enum CorrClass corr_classify(const char *func, const char *detail)
{
	char folded[128];

	if (func == NULL || func[0] == '\0')
		return CORR_CLS_NONE;
	fold(folded, sizeof(folded), func);
	if (name_in(folded, g_probe_funcs))
		return CORR_CLS_PROBE;
	if (name_in(folded, g_write_funcs))
		return CORR_CLS_WRITE;
	if (detail != NULL) {
		char d[256];

		fold(d, sizeof(d), detail);
		if ((strcmp(folded, "createfile") == 0 ||
		     strcmp(folded, "ntcreatefile") == 0) &&
		    strstr(d, "write") != NULL)
			return CORR_CLS_WRITE;
		/* fopen modes: w/a variants write, r variants read. */
		if (strcmp(folded, "fopen") == 0 &&
		    (strchr(d, 'w') != NULL || strchr(d, 'a') != NULL))
			return CORR_CLS_WRITE;
	}
	return CORR_CLS_READ;
}

const char *corr_class_str(enum CorrClass cls)
{
	switch (cls) {
	case CORR_CLS_PROBE:
		return "probe";
	case CORR_CLS_READ:
		return "read";
	case CORR_CLS_WRITE:
		return "write";
	default:
		return "none";
	}
}

/* ----- The inside index (TODO.windows/01-02) ----- */

void corr_index_init(struct CorrIndex *idx)
{
	idx->recs = NULL;
	idx->count = 0;
	idx->cap = 0;
	corr_set_init(&idx->set);
}

static void index_add_record(struct CorrIndex *idx, const char *path,
			     long pid, double t)
{
	struct CorrRec *rec;

	if (idx->count == idx->cap) {
		size_t newcap = (idx->cap == 0) ? 64 : idx->cap * 2;
		struct CorrRec *grown = (struct CorrRec *)realloc(
			idx->recs, newcap * sizeof(*idx->recs));

		if (grown == NULL)
			return;
		idx->recs = grown;
		idx->cap = newcap;
	}
	rec = &idx->recs[idx->count];
	rec->path = (char *)malloc(strlen(path) + 1);
	if (rec->path == NULL)
		return;
	strcpy(rec->path, path);
	rec->pid = pid;
	rec->time = t;
	idx->count++;
	(void)corr_set_add(&idx->set, path);
}

/*
 * One depth-first walk over string values; every path-like
 * string is normalized and handed to the sink. OCP: the index
 * and the per-entry decision both sink here.
 */
static void walk_paths(const JSON_Value *v,
		       void (*sink)(const char *path, void *ctx),
		       void *ctx)
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
				sink(norm, ctx);
		}
		break;
	case JSONObject:
		n = json_object_get_count(json_value_get_object(v));
		for (i = 0; i < n; i++)
			walk_paths(json_object_get_value_at(
					   json_value_get_object(v), i),
				sink, ctx);
		break;
	case JSONArray:
		n = json_array_get_count(json_value_get_array(v));
		for (i = 0; i < n; i++)
			walk_paths(json_array_get_value(
					   json_value_get_array(v), i),
				sink, ctx);
		break;
	default:
		break;
	}
}

struct RecCtx {
	struct CorrIndex *idx;
	long pid;
	double time;
};

static void sink_record(const char *path, void *ctx)
{
	struct RecCtx *rc = (struct RecCtx *)ctx;

	index_add_record(rc->idx, path, rc->pid, rc->time);
}

static void sink_set(const char *path, void *ctx)
{
	(void)corr_set_add((struct CorrSet *)ctx, path);
}

void corr_index_add_entry(JSON_Object *entry, struct CorrIndex *idx)
{
	struct RecCtx rc;

	if (entry == NULL || idx == NULL)
		return;
	rc.idx = idx;
	rc.pid = (long)json_object_get_number(entry, "pid");
	rc.time = json_object_get_number(entry, "time");
	walk_paths(json_object_get_wrapping_value(entry), sink_record,
		&rc);
}

static int cmp_rec(const void *a, const void *b)
{
	const struct CorrRec *ra = (const struct CorrRec *)a;
	const struct CorrRec *rb = (const struct CorrRec *)b;
	int r = corr_pathcmp(ra->path, rb->path);

	if (r != 0)
		return r;
	if (ra->time < rb->time)
		return -1;
	if (ra->time > rb->time)
		return 1;
	return 0;
}

void corr_index_finish(struct CorrIndex *idx)
{
	if (idx->recs != NULL && idx->count > 1)
		qsort(idx->recs, idx->count, sizeof(*idx->recs), cmp_rec);
	corr_set_finish(&idx->set);
}

void corr_index_free(struct CorrIndex *idx)
{
	size_t i;

	for (i = 0; i < idx->count; i++)
		free(idx->recs[i].path);
	free(idx->recs);
	idx->recs = NULL;
	idx->count = 0;
	idx->cap = 0;
	corr_set_free(&idx->set);
}

/* ----- The escape decision ----- */

static int pid_covers(long rec_pid, long pid)
{
	if (rec_pid == 0 || pid == 0)
		return 1; /* pid-less entries are wildcards */
	return rec_pid == pid;
}

/*
 * covered: the path was seen by the inside stream, from a
 * covering pid, within the window when one is set.
 */
static int covered(const struct CorrIndex *idx, const char *path,
		   long pid, double t, double window)
{
	size_t lo = 0, hi = idx->count;

	if (!corr_set_contains(&idx->set, path))
		return 0;

	/* Lower bound of the equal-path record range. */
	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;

		if (corr_pathcmp(idx->recs[mid].path, path) < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	for (; lo < idx->count; lo++) {
		const struct CorrRec *rec = &idx->recs[lo];

		if (corr_pathcmp(rec->path, path) != 0)
			break;
		if (!pid_covers(rec->pid, pid))
			continue;
		if (window > 0) {
			double d = rec->time - t;

			if (d < 0)
				d = -d;
			if (d <= window)
				return 1;
		} else {
			return 1;
		}
	}
	return 0;
}

int corr_entry_is_escape(JSON_Object *entry,
			 const struct CorrCriteria *criteria,
			 const struct CorrIndex *inside,
			 struct CorrEscape *out)
{
	struct CorrSet seen;
	size_t i;

	if (entry == NULL || criteria == NULL || inside == NULL)
		return 0;
	if (criteria->pid != 0 &&
	    criteria->pid != (long)json_object_get_number(entry, "pid"))
		return 0;

	corr_set_init(&seen);
	walk_paths(json_object_get_wrapping_value(entry), sink_set,
		&seen);
	if (seen.count == 0) {
		corr_set_free(&seen);
		return 0;
	}
	for (i = 0; i < seen.count; i++) {
		const char *path = seen.items[i];
		size_t plen = strlen(criteria->prefix);

		if (strncmp(path, criteria->prefix, plen) != 0)
			continue;
		if (path[plen] != '\0' && path[plen] != '/')
			continue;
		if (covered(inside, path,
			    (long)json_object_get_number(entry, "pid"),
			    json_object_get_number(entry, "time"),
			    criteria->window))
			continue;

		{
			JSON_Object *msg = json_object_get_object(entry,
				"message");
			const char *func = msg ?
				json_object_get_string(msg, "func") : NULL;
			const char *detail = msg ?
				json_object_get_string(msg, "detail") :
				NULL;
			enum CorrClass cls = corr_classify(func, detail);

			/* Jail-grant policy: read-attributes leaks are
			 * droppable from the report.
			 */
			if (criteria->exclude_probes &&
			    cls == CORR_CLS_PROBE)
				continue;

			if (out != NULL) {
				snprintf(out->path, sizeof(out->path), "%s",
					path);
				out->func = func;
				out->tid = (long)json_object_get_number(
					entry, "tid");
				out->pid = (long)json_object_get_number(
					entry, "pid");
				out->time = json_object_get_number(entry,
					"time");
				out->cls = cls;
			}
		}
		corr_set_free(&seen);
		return 1;
	}
	corr_set_free(&seen);
	return 0;
}
