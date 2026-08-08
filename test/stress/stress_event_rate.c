/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Stress scenario: sustained event rate (TODO.complete/35 P0).
 *
 * Calls libc functions as fast as possible for a fixed duration
 * (default 10 seconds). Reports events/sec sustained.
 *
 * Under LD_PRELOAD, each libc call fires the trampoline +
 * engine + actions = one "event". This measures the maximum
 * sustained throughput retrace can handle.
 *
 * Default: 10 seconds. Override:
 *   STRESS_SECONDS=N    default 10
 *
 * Part of TODO.complete/35.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_SECONDS 10

int main(void)
{
	const char *sec_env;
	int seconds;
	struct timespec t_start, t_now;
	unsigned long count = 0;
	char buf[32];
	double elapsed;

	sec_env = getenv("STRESS_SECONDS");
	seconds = sec_env ? atoi(sec_env) : DEFAULT_SECONDS;
	if (seconds <= 0 || seconds > 3600)
		seconds = DEFAULT_SECONDS;

	printf("[stress] sustaining for %d seconds...\n", seconds);

	clock_gettime(CLOCK_MONOTONIC, &t_start);

	do {
		(void)getuid();
		snprintf(buf, sizeof(buf), "%lu", count);
		(void)strlen(buf);
		(void)abs((int)count);
		count += 3;

		clock_gettime(CLOCK_MONOTONIC, &t_now);
	} while ((t_now.tv_sec - t_start.tv_sec) < seconds);

	elapsed = (double)(t_now.tv_sec - t_start.tv_sec) +
		  (double)(t_now.tv_nsec - t_start.tv_nsec) / 1e9;

	printf("[stress] PASS: %lu events in %.1f sec = %.0f events/sec\n",
		count, elapsed, (double)count / elapsed);
	return 0;
}
