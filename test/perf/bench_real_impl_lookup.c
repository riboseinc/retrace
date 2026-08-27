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
 * The engine-entry resolve, head to head: the historical
 * dlsym-per-dispatch vs the name-hash cache. The dispatch path
 * calls this on EVERY interposed function call -- the delta is
 * the interposer's per-call overhead budget.
 */

#include <stdio.h>
#include <time.h>

#include "engine.h"
#include "funcs.h"

#define DLSYM_ITERS 100000
#define CACHE_ITERS 10000000

static double now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

int main(void)
{
	double t0, t1, dlsym_ns, cache_ns;
	volatile void *sink = NULL;
	long i;

	t0 = now_ns();
	for (i = 0; i < DLSYM_ITERS; i++)
		sink = retrace_real_impl_resolve("open");
	t1 = now_ns();
	dlsym_ns = (t1 - t0) / DLSYM_ITERS;

	t0 = now_ns();
	for (i = 0; i < CACHE_ITERS; i++)
		sink = retrace_real_impl_cached("open");
	t1 = now_ns();
	cache_ns = (t1 - t0) / CACHE_ITERS;

	printf("resolve(dlsym): %.1f ns/op\n", dlsym_ns);
	printf("cached       : %.1f ns/op\n", cache_ns);
	printf("speedup      : %.1fx\n", dlsym_ns / cache_ns);
	if (cache_ns > dlsym_ns) {
		printf("FAIL: cache slower than dlsym\n");
		return 1;
	}

	/*
	 * The dispatch-tail prototype lookup, same harness: the
	 * historical per-dispatch table walk vs the shared index.
	 */
	{
		double walk_ns, proto_ns;

		t0 = now_ns();
		for (i = 0; i < DLSYM_ITERS; i++)
			sink = retrace_func_get("open");
		t1 = now_ns();
		walk_ns = (t1 - t0) / DLSYM_ITERS;

		t0 = now_ns();
		for (i = 0; i < CACHE_ITERS; i++)
			sink = retrace_proto_cached("open");
		t1 = now_ns();
		proto_ns = (t1 - t0) / CACHE_ITERS;

		printf("proto(walk)  : %.1f ns/op\n", walk_ns);
		printf("proto(cached): %.1f ns/op\n", proto_ns);
		printf("proto speedup: %.1fx\n", walk_ns / proto_ns);
		if (proto_ns > walk_ns) {
			printf("FAIL: proto cache slower than walk\n");
			return 1;
		}
	}
	printf("PASS: real-impl + prototype caches\n");
	return 0;
}
