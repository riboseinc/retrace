/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config_cache.h"

#include <stdlib.h>
#include <string.h>

#include "real_impls.h"
#include "logger.h"

struct cache_entry {
	const char *func_name;
	const JSON_Object *script;
};

struct cache_snap {
	struct cache_entry entries[CONFIG_CACHE_MAX_ENTRIES];
	int count;
};

/*
 * Swap-atomicity (TODO.supervisor/05): policies replace the
 * active config while target threads are dispatching, so the
 * reader must never take a lock -- a frozen wildcard target
 * dispatches millions of calls a second and would starve a
 * locking writer (and vice versa). The builder fills a fresh
 * snapshot and flips ONE atomic pointer; lookups load it once
 * and scan that snapshot. Retired snapshots are retained (the
 * same never-free doctrine as the config trees themselves):
 * swaps are rare control-plane events, and a reader may still
 * be mid-scan.
 */
/*
 * MSVC without C11 atomics falls back to a volatile pointer: an
 * aligned pointer store/load is atomic on every supported arch.
 */
#if defined(__STDC_NO_ATOMICS__)
static struct cache_snap *volatile g_snap;

static struct cache_snap *snap_load(void)
{
	return g_snap;
}

static void snap_store(struct cache_snap *p)
{
	g_snap = p;
}
#else
#include <stdatomic.h>

static _Atomic(struct cache_snap *) g_snap;

static struct cache_snap *snap_load(void)
{
	return atomic_load(&g_snap);
}

static void snap_store(struct cache_snap *p)
{
	atomic_store(&g_snap, p);
}
#endif

int retrace_config_cache_build(JSON_Object *conf)
{
	JSON_Array *scripts;
	size_t i, n;

	if (conf == NULL) {
		log_err("config_cache: conf is NULL");
		return -1;
	}

	scripts = json_object_get_array(conf, "intercept_scripts");
	if (scripts == NULL) {
		log_err("config_cache: no intercept_scripts in conf");
		return -1;
	}

	{
		struct cache_snap *snap = calloc(1, sizeof(*snap));

		if (snap == NULL) {
			log_err("config_cache: out of memory");
			return -1;
		}
		n = json_array_get_count(scripts);
		for (i = 0; i < n && snap->count < CONFIG_CACHE_MAX_ENTRIES;
		     i++) {
			const JSON_Object *script =
				json_array_get_object(scripts, i);
			const char *name;

			if (script == NULL)
				continue;

			name = json_object_get_string(script, "func_name");
			if (name == NULL || name[0] == '\0')
				continue;

			if (retrace_real_impls.strcmp(name, "*") == 0)
				continue;

			snap->entries[snap->count].func_name = name;
			snap->entries[snap->count].script = script;
			snap->count++;
		}
		snap_store(snap);
	}

	log_info("config_cache: built with %d entries (%zu scripts total, %zu wildcards skipped)",
		retrace_config_cache_count(), n,
		n - (size_t)retrace_config_cache_count());

	return 0;
}

const JSON_Object *retrace_config_cache_lookup(const char *func_name)
{
	const struct cache_snap *snap = snap_load();
	const JSON_Object *hit = NULL;
	int i;

	if (func_name == NULL || snap == NULL)
		return NULL;

	for (i = 0; i < snap->count; i++) {
		if (retrace_real_impls.strcmp(snap->entries[i].func_name,
			    func_name) == 0) {
			hit = snap->entries[i].script;
			break;
		}
	}
	return hit;
}

void retrace_config_cache_clear(void)
{
	struct cache_snap *empty = calloc(1, sizeof(*empty));

	if (empty == NULL)
		return;
	snap_store(empty);
}

int retrace_config_cache_count(void)
{
	const struct cache_snap *snap = snap_load();

	return snap != NULL ? snap->count : 0;
}
