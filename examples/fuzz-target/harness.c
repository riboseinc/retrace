/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * libFuzzer harness template (TODO.trace-profile/20): the
 * retrace JSON config drives what happens to each fuzzer input
 * BEFORE the target parses it -- mutate, inject, or fail the
 * underlying libc calls -- while libFuzzer's coverage feedback
 * (plus retrace's call_hash) explores. The TARGET FUNCTION is
 * the only part you replace.
 *
 * build:  clang -g -O1 -fsanitize=fuzzer -o fuzz harness.c
 * run:    ./fuzz corpus/        (seed corpus from the workbench)
 *
 * Pair with the workbench: corpus minimized by
 * retrace-fuzz-report feeds this harness's corpus dir; a config
 * with RETRACE_FUZZ_SEED determinism reproduces any finding.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*
 * TARGET: replace this with the parser under test. It must be
 * deterministic and memory-safe BY CONTRACT -- the harness
 * supplies hostile inputs (truncated, embedded-NUL, oversized)
 * via the mutations below.
 */
static int target_parse(const uint8_t *data, size_t size)
{
	/* example: a header parser that crashes on a bad length */
	uint16_t len;

	if (size < 2)
		return -1;
	memcpy(&len, data, 2);
	if (len > size - 2)
		return -1; /* correct rejection */
	return data[2 + len - 1]; /* demo sink */
}

/*
 * HARNESS MUTATIONS (v1): deterministic input shaping before
 * the target. Extend with your grammar; the retrace layer adds
 * libc-level fault injection via the JSON config.
 */
static size_t mutate(uint8_t *buf, size_t size)
{
	/* embedded NUL: parsers that stop at NUL miss the tail */
	if (size > 4)
		buf[size / 2] = 0;
	return size;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	uint8_t buf[4096];
	size_t n = size < sizeof(buf) ? size : sizeof(buf);

	memcpy(buf, data, n);
	(void)target_parse(buf, mutate(buf, n));
	return 0;
}
