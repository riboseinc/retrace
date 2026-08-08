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

#include <string.h>

#include "real_impls.h"
#include "logger.h"

struct cache_entry {
	const char *func_name;
	const JSON_Object *script;
};

static struct cache_entry g_entries[CONFIG_CACHE_MAX_ENTRIES];
static int g_count;

int retrace_config_cache_build(JSON_Object *conf)
{
	JSON_Array *scripts;
	size_t i, n;

	if (conf == NULL) {
		log_err("config_cache: conf is NULL");
		return -1;
	}

	g_count = 0;

	scripts = json_object_get_array(conf, "intercept_scripts");
	if (scripts == NULL) {
		log_err("config_cache: no intercept_scripts in conf");
		return -1;
	}

	n = json_array_get_count(scripts);
	for (i = 0; i < n && g_count < CONFIG_CACHE_MAX_ENTRIES; i++) {
		const JSON_Object *script = json_array_get_object(scripts, i);
		const char *name;

		if (script == NULL)
			continue;

		name = json_object_get_string(script, "func_name");
		if (name == NULL || name[0] == '\0')
			continue;

		if (retrace_real_impls.strcmp(name, "*") == 0)
			continue;

		g_entries[g_count].func_name = name;
		g_entries[g_count].script = script;
		g_count++;
	}

	log_info("config_cache: built with %d entries (%zu scripts total, %zu wildcards skipped)",
		g_count, n, n - (size_t)g_count);

	return 0;
}

const JSON_Object *retrace_config_cache_lookup(const char *func_name)
{
	int i;

	if (func_name == NULL)
		return NULL;

	for (i = 0; i < g_count; i++) {
		if (retrace_real_impls.strcmp(g_entries[i].func_name,
			    func_name) == 0)
			return g_entries[i].script;
	}

	return NULL;
}

void retrace_config_cache_clear(void)
{
	int i;

	for (i = 0; i < g_count; i++) {
		g_entries[i].func_name = NULL;
		g_entries[i].script = NULL;
	}
	g_count = 0;
}

int retrace_config_cache_count(void)
{
	return g_count;
}
