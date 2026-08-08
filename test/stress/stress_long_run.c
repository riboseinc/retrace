/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Stress scenario: long-running target (TODO.complete/35 P0).
 *
 * Runs for a configurable duration (default 60 seconds),
 * continuously calling libc functions. Designed for nightly CI:
 * verify retrace doesn't degrade over time (memory leaks,
 * counter overflows, log corruption).
 *
 * Default: 60 seconds. Override:
 *   STRESS_SECONDS=N    default 60
 *
 * Part of TODO.complete/35.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_SECONDS 60

int main(void)
{
	const char *sec_env;
	int seconds;
	struct timespec t_start, t_now;
	unsigned long iter = 0;
	char buf[64];

	sec_env = getenv("STRESS_SECONDS");
	seconds = sec_env ? atoi(sec_env) : DEFAULT_SECONDS;
	if (seconds <= 0 || seconds > 86400)
		seconds = DEFAULT_SECONDS;

	printf("[stress] running for %d seconds...\n", seconds);

	clock_gettime(CLOCK_MONOTONIC, &t_start);

	do {
		(void)getuid();
		(void)abs((int)iter);
		snprintf(buf, sizeof(buf), "iter-%lu", iter);
		(void)strlen(buf);
		iter++;

		if (iter % 1000000 == 0) {
			clock_gettime(CLOCK_MONOTONIC, &t_now);
			printf("[stress] %lu iterations, %ld sec elapsed\n",
				iter,
				(long)(t_now.tv_sec - t_start.tv_sec));
		}

		clock_gettime(CLOCK_MONOTONIC, &t_now);
	} while ((t_now.tv_sec - t_start.tv_sec) < seconds);

	printf("[stress] PASS: %lu iterations in %d sec\n",
		iter, seconds);
	return 0;
}
