/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE.md for details.
 */

/*
 * End-to-end dispatch-overhead benchmark (GH #866, card 22).
 *
 * The F2 question: does the dispatch TAIL (entry + name lookup
 * + guard + tail call, with no actions) dominate a realistic
 * dispatch? The in-process lookup is 22ns (bench_real_impl_
 * lookup); actions cost 5-7us. But the tail's real share needs
 * END-TO-END numbers -- interposed calls through the actual
 * trampoline, not engine-internal microbenches.
 *
 * This binary runs the SAME tight call loop three ways; the
 * retrace library (when preloaded) dispatches per config:
 *
 *   ./bench_dispatch_overhead none    - no retrace config at all
 *   ./bench_dispatch_overhead nomatch - config matches nothing
 *   ./bench_dispatch_overhead capture - log_params + call_real
 *
 * Without the preload all three are identical (sanity). The
 * runner script compares: tail = nomatch - none;
 * actions = capture - nomatch. The F2 verdict:
 *   tail / (tail + actions) > 0.5  -> the tail dominates.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ITERS 2000000

static double now_s(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
	const char *mode = argc > 1 ? argv[1] : "none";
	volatile size_t sink = 0;
	char buf[32];
	double t0;
	size_t i;


	/* the workload: cheap, plentiful libc calls -- the class
	 * a farm target makes millions of; strlen and malloc/free
	 * are both in the intercept inventory
	 */
	memset(buf, 'x', sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';

	t0 = now_s();
	for (i = 0; i < ITERS; i++) {
		sink += strlen(buf);
		if ((i & 1) == 0) {
			char *m = malloc(32);

			if (m != NULL) {
				m[0] = (char) i;
				sink += (size_t) m[0];
				free(m);
			}
		}
	}
	{
		double dt = now_s() - t0;

		/* stderr: under the preload the result line must not
		 * ride the same stdio the logger writes to */
		fprintf(stderr,
			"mode=%s iters=%d seconds=%.4f ns_per_call=%.1f ns_int=%.0f sink=%zu\n",
			mode, ITERS, dt,
			dt * 1e9 / (double) (ITERS + ITERS / 2),
			dt * 1e9 / (double) (ITERS + ITERS / 2),
			(size_t) sink);
	}
	return 0;
}
