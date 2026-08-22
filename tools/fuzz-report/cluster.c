/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Crash clustering (TODO.trace-profile/20). See cluster.h. The
 * signature hash is FNV-1a over (func name, param count) -- a
 * stable, dependency-free digest.
 */

#include "cluster.h"
#include "stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/wait.h>
#define FUZZ_SIGNALED(st) WIFSIGNALED(st)
#define FUZZ_SIG(st) WTERMSIG(st)
#else
#define FUZZ_SIGNALED(st) 0
#define FUZZ_SIG(st) 0
#endif

static unsigned long fnv1a(const char *s, unsigned long h)
{
	for (; *s != '\0'; s++) {
		h ^= (unsigned char)*s;
		h *= 0x01000193UL;
	}
	return h;
}

void fuzz_report_init(struct FuzzReport *r)
{
	memset(r, 0, sizeof(*r));
}

void fuzz_report_free(struct FuzzReport *r)
{
	free(r->clusters);
	fuzz_report_init(r);
}

static struct FuzzCluster *find_cluster(struct FuzzReport *r,
					unsigned long id)
{
	size_t i;

	for (i = 0; i < r->count; i++) {
		if (r->clusters[i].id == id)
			return &r->clusters[i];
	}
	return NULL;
}

static struct FuzzCluster *add_cluster(struct FuzzReport *r,
					unsigned long id, const char *func,
					int params, int is_crash)
{
	struct FuzzCluster *c;

	if (r->count == r->cap) {
		size_t newcap = r->cap == 0 ? 8 : r->cap * 2;
		struct FuzzCluster *grown = (struct FuzzCluster *)
			realloc(r->clusters,
				newcap * sizeof(*grown));

		if (grown == NULL)
			return NULL;
		r->clusters = grown;
		r->cap = newcap;
	}
	c = &r->clusters[r->count++];
	memset(c, 0, sizeof(*c));
	c->id = id;
	snprintf(c->func, sizeof(c->func), "%s",
		func != NULL ? func : "?");
	c->params = params;
	c->is_crash = is_crash;
	return c;
}

/*
 * last func entry in a trace: the tolerant scanner (stream.c)
 * walks entries even when a SIGSEGV truncated the file mid-
 * write -- the raw parson parse would fail on exactly the
 * traces we care about. Remember the newest message with a
 * "func" key (return summaries re-state func: fine, they name
 * the call that was in flight when the target died).
 */
struct last_func_ctx {
	char func[64];
	int params;
};

static void last_func_cb(JSON_Object *entry, void *ud)
{
	struct last_func_ctx *ctx = (struct last_func_ctx *)ud;
	JSON_Object *msg = json_object_get_object(entry, "message");
	const char *f = msg != NULL ?
		json_object_get_string(msg, "func") : NULL;
	JSON_Object *po;

	if (f == NULL || f[0] == '\0')
		return;
	snprintf(ctx->func, sizeof(ctx->func), "%s", f);
	po = json_object_get_object(msg, "params");
	ctx->params = po != NULL ? (int)json_object_get_count(po) : 0;
}

static void last_func(const char *trace_json, char *func,
		      size_t funcsz, int *params)
{
	struct last_func_ctx ctx;
	size_t skipped = 0;

	memset(&ctx, 0, sizeof(ctx));
	if (trace_json != NULL)
		corr_stream_scan(trace_json, strlen(trace_json),
			last_func_cb, &ctx, &skipped);
	/* died before any entry flushed: a distinct, honest
	 * cluster ("?" -- unattributable), never merged with a
	 * named function
	 */
	snprintf(func, funcsz, "%s",
		ctx.func[0] != '\0' ? ctx.func : "?");
	*params = ctx.params;
}

unsigned long fuzz_report_fold(struct FuzzReport *r, int exit_status,
	const char *trace_json, unsigned long seed, const char *marker)
{
	char func[64];
	int params = 0;
	unsigned long id;
	struct FuzzCluster *c;
	int is_crash = FUZZ_SIGNALED(exit_status);
	int is_assert = 0;

	r->total++;
	if (is_crash) {
		r->crashes++;
	} else if (marker != NULL && trace_json != NULL &&
		   strstr(trace_json, marker) != NULL) {
		is_assert = 1;
		r->assertions++;
	}
	if (!is_crash && !is_assert)
		return 0; /* clean run: not clustered */

	last_func(trace_json, func, sizeof(func), &params);
	id = fnv1a(func, 0x811C9DC5UL);
	id ^= (unsigned long)params * 0x9E3779B97F4A7C15UL;
	id *= 0x01000193UL;

	c = find_cluster(r, id);
	if (c == NULL) {
		c = add_cluster(r, id, func, params, is_crash);
		if (c == NULL)
			return id;
		c->first_seed = seed;
	}
	c->count++;
	return id;
}

JSON_Value *fuzz_report_to_json(const struct FuzzReport *r)
{
	JSON_Value *v = json_value_init_object();
	JSON_Object *o = json_value_get_object(v);
	JSON_Value *arr = json_value_init_array();
	size_t i;

	json_object_set_number(o, "iterations", (double)r->total);
	json_object_set_number(o, "crashes", (double)r->crashes);
	json_object_set_number(o, "assertions", (double)r->assertions);

	for (i = 0; i < r->count; i++) {
		const struct FuzzCluster *c = &r->clusters[i];
		JSON_Value *cv = json_value_init_object();
		JSON_Object *co = json_value_get_object(cv);

		{
			char id_s[32];

			snprintf(id_s, sizeof(id_s), "%lu", c->id);
			json_object_set_string(co, "id", id_s);
		}
		json_object_set_string(co, "func", c->func);
		json_object_set_number(co, "params", (double)c->params);
		json_object_set_number(co, "count", (double)c->count);
		{
			char seed_s[32];

			snprintf(seed_s, sizeof(seed_s), "%lu",
				c->first_seed);
			json_object_set_string(co, "seed", seed_s);
		}
		json_object_set_string(co, "kind", c->is_crash ?
			"crash" : "assertion");
		json_array_append_value(json_value_get_array(arr), cv);
	}
	json_object_set_value(o, "clusters", arr);
	return v;
}
