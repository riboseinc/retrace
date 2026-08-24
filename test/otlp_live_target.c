/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * A test target for the otlp-c live streaming integration test
 * (TODO.trace-profile/31). Generates enough traced calls to give
 * the otlp-c exporter's background thread time to establish a
 * connection and POST at least one batch.
 *
 * Pure C, no engine deps. Linked into the test/ tree so ctest can
 * pass its path to the integration test.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOOP_COUNT 300
#define BUF_SIZE 64

int main(void)
{
	int i;
	char buf[BUF_SIZE];

	/* Mix of malloc/free/printf so the trace has variety. */
	for (i = 0; i < LOOP_COUNT; i++) {
		void *p = malloc(32);

		if (p != NULL) {
			snprintf(buf, BUF_SIZE, "iter-%d", i);
			printf("otlp-test: %s\n", buf);
			free(p);
		}
		/* Brief sleep so the otlp-c tick (100ms) gets a chance
		 * to run before we exit. ~1.5s total on commodity
		 * hardware -- the first batch posts well inside that
		 * window; slow CI runners get proportionally slower but
		 * only ONE POST is required to pass.
		 */
		usleep(5000);
	}
	return 0;
}
