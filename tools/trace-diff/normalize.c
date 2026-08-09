/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "normalize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Set a fixed-size buffer from a string. Truncates if too long.
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

/*
 * Find or create a FuncStat entry by name. Linear scan is fine --
 * normalized logs are typically O(100) entries.
 */
static struct FuncStat *log_get_or_create(struct NormalizedLog *log,
					  const char *name)
{
	size_t i;
	struct FuncStat *newbuf;

	if (log == NULL || name == NULL)
		return NULL;

	for (i = 0; i < log->count; i++) {
		if (strcmp(log->funcs[i].name, name) == 0)
			return &log->funcs[i];
	}

	if (log->count == log->cap) {
		size_t newcap = (log->cap == 0) ? 16 : log->cap * 2;

		newbuf = (struct FuncStat *)realloc(log->funcs,
			newcap * sizeof(*newbuf));
		if (newbuf == NULL)
			return NULL;
		log->funcs = newbuf;
		log->cap = newcap;
	}

	set_fixed(log->funcs[log->count].name,
		sizeof(log->funcs[log->count].name), name);
	log->funcs[log->count].call_count = 0;
	log->funcs[log->count].total_duration_us = 0;
	return &log->funcs[log->count++];
}

int normalize_from_trace(JSON_Array *trace, struct NormalizedLog *out)
{
	size_t i, n;

	if (trace == NULL || out == NULL)
		return -1;

	memset(out, 0, sizeof(*out));

	n = json_array_get_count(trace);
	for (i = 0; i < n; i++) {
		JSON_Object *entry = json_array_get_object(trace, i);
		JSON_Object *msg;
		const char *func;
		struct FuncStat *st;

		if (entry == NULL)
			continue;
		msg = json_object_get_object(entry, "message");
		if (msg == NULL)
			continue;

		/* Skip engine-noise entries (those with "text" instead of
		 * "func" + "call_duration_us"). The OTLP converter does
		 * the same filter.
		 */
		func = json_object_get_string(msg, "func");
		if (func == NULL || !json_object_has_value(msg,
			    "call_duration_us"))
			continue;

		st = log_get_or_create(out, func);
		if (st == NULL)
			return -1;
		st->call_count++;
		st->total_duration_us += (uint64_t)json_object_get_number(msg,
			"call_duration_us");
	}

	return 0;
}

const struct FuncStat *normalize_find(const struct NormalizedLog *log,
				      const char *name)
{
	size_t i;

	if (log == NULL || name == NULL)
		return NULL;
	for (i = 0; i < log->count; i++) {
		if (strcmp(log->funcs[i].name, name) == 0)
			return &log->funcs[i];
	}
	return NULL;
}

void normalize_free(struct NormalizedLog *log)
{
	if (log == NULL)
		return;
	free(log->funcs);
	log->funcs = NULL;
	log->count = 0;
	log->cap = 0;
}
