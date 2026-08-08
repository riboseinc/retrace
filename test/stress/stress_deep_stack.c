/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Stress scenario: deep call stack (TODO.complete/35 P0).
 *
 * A recursive function that calls libc at each level, going
 * DEPTH levels deep. Under LD_PRELOAD, each libc call fires
 * the trampoline + engine, stressing:
 *   - Per-thread context allocation/reuse
 *   - Stack depth of the trampoline chain
 *   - Reentrance guard correctness at every level
 *
 * Default: 100 levels. Override via:
 *   STRESS_DEPTH=N    default 100
 *
 * Part of TODO.complete/35.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_DEPTH 100

static void recurse(int depth, int max)
{
	char buf[32];

	if (depth >= max)
		return;

	snprintf(buf, sizeof(buf), "depth-%d", depth);
	(void)strlen(buf);
	(void)getuid();

	recurse(depth + 1, max);
}

int main(void)
{
	const char *depth_env;
	int max_depth;

	depth_env = getenv("STRESS_DEPTH");
	max_depth = depth_env ? atoi(depth_env) : DEFAULT_DEPTH;
	if (max_depth <= 0 || max_depth > 10000)
		max_depth = DEFAULT_DEPTH;

	printf("[stress] recursing to depth %d...\n", max_depth);

	recurse(0, max_depth);

	printf("[stress] PASS: depth %d\n", max_depth);
	return 0;
}
