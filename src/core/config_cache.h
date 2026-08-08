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

/*
 * Config cache: pre-resolves func_name -> JSON_Object* at init
 * time so the script_resolver can skip the JSON array walk for
 * exact-name matches (TODO.complete/18 MVP).
 *
 * The cache stores pointers into the existing JSON tree (no
 * copies). The JSON tree is owned by retrace_conf which lives
 * for the process's lifetime, so the pointers are stable.
 *
 * Wildcard ("*") entries are NOT cached -- they require
 * runtime evaluation that depends on call-site context. The
 * resolver checks the cache first (O(1) for exact match),
 * then falls through to the JSON walk for wildcards.
 *
 * Design rationale: separating "compile" (config parse +
 * cache build) from "execute" (per-call lookup) is the
 * canonical pattern for every modern tracing tool. Even
 * though the current JSON walk is fast (sub-us for typical
 * configs), the cache:
 *   - Makes the compile/execute boundary explicit
 *   - Enables future optimizations (hash table, perfect
 *     hashing) without touching the resolver
 *   - Makes config validation testable in isolation
 */

#ifndef RETRACE_CORE_CONFIG_CACHE_H_
#define RETRACE_CORE_CONFIG_CACHE_H_

#include "parson.h"

#define CONFIG_CACHE_MAX_ENTRIES 256

/*
 * Build the cache from the given retrace_conf object. Called
 * once after retrace_conf_init(). Safe to call multiple times
 * (clears + rebuilds).
 *
 * Returns 0 on success, -1 if the conf has no
 * intercept_scripts array.
 */
int retrace_config_cache_build(JSON_Object *conf);

/*
 * Look up func_name in the cache. Returns the matching
 * JSON_Object* (script entry) on hit, NULL on miss.
 *
 * Only matches exact func_name (no wildcards). The caller
 * is responsible for evaluating caller_matches / return_addr
 * on the returned script.
 */
const JSON_Object *retrace_config_cache_lookup(const char *func_name);

/*
 * Clear the cache. Test-only; used between test cases.
 */
void retrace_config_cache_clear(void);

/*
 * Return the number of entries in the cache (exact-name
 * scripts only, excludes wildcards). Test/diagnostic.
 */
int retrace_config_cache_count(void);

#endif /* RETRACE_CORE_CONFIG_CACHE_H_ */
