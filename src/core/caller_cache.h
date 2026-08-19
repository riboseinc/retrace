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
 * dladdr result cache for caller_match (TODO.complete/17 P1).
 *
 * dladdr is ~10us per call on macOS -- tolerable for one-off
 * config scripts but a real cost on hot paths. The cache stores
 * the most recent N (ret_addr -> rc_dl_info_t) mappings so repeat
 * lookups for the same address are O(1).
 *
 * Cache is process-global and mutex-protected. Threads can
 * call caller_match concurrently. The mutex is held briefly
 * (linear scan of N entries + one insert); contention is
 * negligible in practice.
 *
 * No eviction: retrace processes are short-lived. If the cache
 * fills, the new entry overwrites the oldest slot (FIFO-ish via
 * a circular index).
 *
 * Cache hits skip dladdr entirely. Misses call dladdr and store
 * the result. The cache is sized to fit common cases (256
 * entries covers most call sites in a typical binary).
 */

#ifndef RETRACE_CORE_CALLER_CACHE_H_
#define RETRACE_CORE_CALLER_CACHE_H_

#include "posix_compat.h"

/*
 * Look up ret_addr in the cache. On hit, returns 1 and writes
 * the cached info to *out. On miss, returns 0 (caller should
 * call dladdr and then caller_cache_insert).
 *
 * Thread-safe.
 */
int retrace_caller_cache_lookup(void *ret_addr, rc_dl_info_t *out);

/*
 * Insert a (ret_addr -> info) mapping. Safe to call with the
 * same ret_addr multiple times -- the latest info wins.
 *
 * Thread-safe.
 */
void retrace_caller_cache_insert(void *ret_addr, const rc_dl_info_t *info);

/*
 * Test-only: clear the cache and reset stats. Used by unit
 * tests to verify hit/miss behavior.
 */
void retrace_caller_cache_clear(void);

/*
 * Test-only: read the running hit/miss counters. Used by unit
 * tests and the perf benchmark to verify the cache is working.
 */
void retrace_caller_cache_stats(unsigned long *hits,
				unsigned long *misses);

#endif /* RETRACE_CORE_CALLER_CACHE_H_ */
