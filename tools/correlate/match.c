/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "match.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
corr_is_path_like(const char *s)
{
	size_t n;

	if (s == NULL || s[0] == '\0')
		return 0;
	n = strlen(s);
	if (s[0] == '/' || s[0] == '\\')
		return 1;
	if (n >= 2 && ((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z')) &&
	    s[1] == ':')
		return 1;
	return strchr(s, '/') != NULL || strchr(s, '\\') != NULL;
}

size_t
corr_normalize(const char *in, char *out, size_t outsz)
{
	const char *p = in;
	char	    drive[2] = {0, 0};
	size_t	    prefix = 0;
	size_t	    n, i;

	if (in == NULL || out == NULL || outsz == 0)
		return 0;
	out[0] = '\0';

	/* NT prefix forms -> plain path. */
	if (strncmp(p, "\\??\\", 4) == 0) {
		p += 4;
	} else if (strncmp(p, "\\\\?\\", 4) == 0) {
		p += 4;
	} else if (strncmp(p, "\\Device\\HarddiskVolume", 22) == 0) {
		/* "\Device\HarddiskVolume3\rest" -> guess a DOS drive
		 * letter (volume N -> 'A'+N-1, so volume 3 -> C:, the
		 * first visible volume on a standard install). Both
		 * sides of the join normalize through this function,
		 * so consistency matters more than accuracy.
		 */
		const char *v = p + 22;
		long	    vol = 0;

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

int
corr_pathcmp(const char *a, const char *b)
{
	if (a == NULL || b == NULL)
		return (a == b) ? 0 : (a == NULL ? -1 : 1);
	/* Drive letter case-insensitive; rest case-sensitive. */
	if (((a[0] >= 'A' && a[0] <= 'Z') || (a[0] >= 'a' && a[0] <= 'z')) && a[1] == ':' &&
	    ((b[0] >= 'A' && b[0] <= 'Z') || (b[0] >= 'a' && b[0] <= 'z')) && b[1] == ':') {
		char ca = a[0];
		char cb = b[0];
		int  r;

		if (ca >= 'a' && ca <= 'z')
			ca = (char) (ca - 'a' + 'A');
		if (cb >= 'a' && cb <= 'z')
			cb = (char) (cb - 'a' + 'A');
		r = (int) (unsigned char) ca - (int) (unsigned char) cb;
		if (r != 0)
			return r;
		a += 2;
		b += 2;
	}
	return strcmp(a, b);
}

void
corr_set_init(struct CorrSet *s)
{
	s->items = NULL;
	s->count = 0;
	s->cap = 0;
}

static int
cmp_setitem(const void *a, const void *b)
{
	return corr_pathcmp(*(const char *const *) a, *(const char *const *) b);
}

int
corr_set_add(struct CorrSet *s, const char *path)
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
		char **grown = (char **) realloc(s->items, newcap * sizeof(char *));

		if (grown == NULL)
			return -1;
		s->items = grown;
		s->cap = newcap;
	}
	copy = (char *) malloc(strlen(path) + 1);
	if (copy == NULL)
		return -1;
	strcpy(copy, path);
	s->items[s->count++] = copy;
	return 0;
}

void
corr_set_finish(struct CorrSet *s)
{
	if (s->items != NULL && s->count > 1)
		qsort(s->items, s->count, sizeof(char *), cmp_setitem);
}

int
corr_set_contains(const struct CorrSet *s, const char *path)
{
	size_t lo = 0, hi = s->count;

	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		int    r = corr_pathcmp(s->items[mid], path);

		if (r == 0)
			return 1;
		if (r < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	return 0;
}

void
corr_set_free(struct CorrSet *s)
{
	size_t i;

	for (i = 0; i < s->count; i++)
		free(s->items[i]);
	free(s->items);
	s->items = NULL;
	s->count = 0;
	s->cap = 0;
}

/* Depth-first string-value walk. */
static void
collect_strings(const JSON_Value *v, struct CorrSet *out, int *added)
{
	size_t i, n;

	if (v == NULL)
		return;
	switch (json_value_get_type(v)) {
	case JSONString:
		if (corr_is_path_like(json_value_get_string(v))) {
			char norm[CORR_PATH_MAX];

			if (corr_normalize(json_value_get_string(v), norm, sizeof(norm)) > 0 &&
			    corr_set_add(out, norm) == 0)
				(*added)++;
		}
		break;
	case JSONObject:
		n = json_object_get_count(json_value_get_object(v));
		for (i = 0; i < n; i++)
			collect_strings(
			  json_object_get_value_at(json_value_get_object(v), i), out, added);
		break;
	case JSONArray:
		n = json_array_get_count(json_value_get_array(v));
		for (i = 0; i < n; i++)
			collect_strings(
			  json_array_get_value(json_value_get_array(v), i), out, added);
		break;
	default:
		break;
	}
}

int
corr_collect_paths(JSON_Object *entry, struct CorrSet *out)
{
	int added = 0;

	if (entry == NULL)
		return 0;
	collect_strings(json_object_get_wrapping_value(entry), out, &added);
	return added;
}

int
corr_entry_is_escape(JSON_Object *entry,
		     const char *prefix,
		     const struct CorrSet *inside,
		     struct CorrEscape *out)
{
	struct CorrSet seen;
	size_t	       i;

	if (entry == NULL || prefix == NULL || inside == NULL)
		return 0;

	corr_set_init(&seen);
	if (corr_collect_paths(entry, &seen) == 0) {
		corr_set_free(&seen);
		return 0;
	}
	for (i = 0; i < seen.count; i++) {
		const char *path = seen.items[i];
		size_t	    plen = strlen(prefix);

		if (strncmp(path, prefix, plen) != 0)
			continue;
		if (path[plen] != '\0' && path[plen] != '/')
			continue;
		if (corr_set_contains(inside, path))
			continue;
		if (out != NULL) {
			JSON_Object *msg = json_object_get_object(entry, "message");
			const char *func = msg ? json_object_get_string(msg, "func") : NULL;

			snprintf(out->path, sizeof(out->path), "%s", path);
			out->func = func;
			out->tid = (long) json_object_get_number(entry, "tid");
		}
		corr_set_free(&seen);
		return 1;
	}
	corr_set_free(&seen);
	return 0;
}
