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

#include "caller_cache.h"

#include <string.h>

#include "real_impls.h"
#include "posix_compat.h"

/*
 * Cache layout. 256 entries × ~300 bytes each ~= 77 KB. Fits
 * comfortably in L2. Lookup is a linear scan of ret_addr fields
 * -- cache-line friendly (no pointer chase).
 *
 * Slot 0 is reserved as the "empty" sentinel: ret_addr == NULL
 * means empty. Real ret_addr values from intercepted calls are
 * always non-NULL.
 */
#define CALLER_CACHE_SIZE 256

struct caller_cache_entry {
	void *ret_addr;
	rc_dl_info_t info;
};

static struct caller_cache_entry g_cache[CALLER_CACHE_SIZE];
static rc_mutex_t g_cache_lock = RC_MUTEX_STATIC_INIT;
static unsigned long g_cache_hits;
static unsigned long g_cache_misses;

int retrace_caller_cache_lookup(void *ret_addr, rc_dl_info_t *out)
{
	int found = 0;
	size_t i;

	if (ret_addr == NULL || out == NULL)
		return 0;

	retrace_real_impls.rc_mutex_lock(&g_cache_lock);

	for (i = 0; i < CALLER_CACHE_SIZE; i++) {
		if (g_cache[i].ret_addr == ret_addr) {
			*out = g_cache[i].info;
			g_cache_hits++;
			found = 1;
			break;
		}
	}

	if (!found)
		g_cache_misses++;

	retrace_real_impls.rc_mutex_unlock(&g_cache_lock);
	return found;
}

void retrace_caller_cache_insert(void *ret_addr, const rc_dl_info_t *info)
{
	size_t i;
	size_t slot = 0;
	int found = 0;

	if (ret_addr == NULL || info == NULL)
		return;

	retrace_real_impls.rc_mutex_lock(&g_cache_lock);

	/* Update existing entry if present. */
	for (i = 0; i < CALLER_CACHE_SIZE; i++) {
		if (g_cache[i].ret_addr == ret_addr) {
			slot = i;
			found = 1;
			break;
		}
	}

	/* Otherwise claim first empty slot. */
	if (!found) {
		for (i = 0; i < CALLER_CACHE_SIZE; i++) {
			if (g_cache[i].ret_addr == NULL) {
				slot = i;
				found = 1;
				break;
			}
		}
	}

	/* If table is full, overwrite slot 0. Caller matches are
	 * best-effort under load; overwriting is preferable to
	 * blocking on a more elaborate eviction scheme.
	 */
	if (!found)
		slot = 0;

	g_cache[slot].ret_addr = ret_addr;
	g_cache[slot].info = *info;

	retrace_real_impls.rc_mutex_unlock(&g_cache_lock);
}

void retrace_caller_cache_clear(void)
{
	size_t i;

	retrace_real_impls.rc_mutex_lock(&g_cache_lock);
	for (i = 0; i < CALLER_CACHE_SIZE; i++)
		g_cache[i].ret_addr = NULL;
	g_cache_hits = 0;
	g_cache_misses = 0;
	retrace_real_impls.rc_mutex_unlock(&g_cache_lock);
}

void retrace_caller_cache_stats(unsigned long *hits, unsigned long *misses)
{
	retrace_real_impls.rc_mutex_lock(&g_cache_lock);
	if (hits)
		*hits = g_cache_hits;
	if (misses)
		*misses = g_cache_misses;
	retrace_real_impls.rc_mutex_unlock(&g_cache_lock);
}
