/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Stress scenario: large argument buffers (TODO.complete/35 P0).
 *
 * Allocates progressively larger buffers and passes them to
 * memset/memcpy. Under LD_PRELOAD, retrace intercepts each
 * call and walks the buffer for logging (if log_params is
 * configured).
 *
 * Stress points:
 *   - log_params serialization cost scales with buffer size
 *   - Memory pressure from large allocations
 *   - Potential integer overflow in size calculations
 *
 * Default: 10 sizes from 1KB to 1MB. Override:
 *   STRESS_MAX_BYTES=N    default 1048576 (1 MB)
 *
 * Part of TODO.complete/35.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_MAX_BYTES (1024 * 1024)

int main(void)
{
	const char *max_env;
	size_t max_bytes;
	size_t sz;
	unsigned char *buf;

	max_env = getenv("STRESS_MAX_BYTES");
	max_bytes = max_env ? (size_t)atol(max_env) : DEFAULT_MAX_BYTES;
	if (max_bytes < 1024 || max_bytes > 100 * 1024 * 1024)
		max_bytes = DEFAULT_MAX_BYTES;

	printf("[stress] testing buffer sizes up to %zu bytes\n",
		max_bytes);

	for (sz = 1024; sz <= max_bytes; sz *= 4) {
		buf = (unsigned char *)malloc(sz);
		if (buf == NULL) {
			fprintf(stderr,
				"[stress] FAIL: malloc %zu\n", sz);
			return 1;
		}

		memset(buf, 0xAB, sz);
		memcpy(buf + sz / 2, buf, sz / 2);
		(void)strlen((char *)buf);

		free(buf);
	}

	printf("[stress] PASS: up to %zu bytes\n", max_bytes);
	return 0;
}
