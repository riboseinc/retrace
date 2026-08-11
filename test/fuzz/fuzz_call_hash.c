/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * libFuzzer custom mutator using retrace's call_hash as
 * coverage feedback (TODO.complete/24 P1).
 *
 * This harness defines LLVMFuzzerCustomMutator. After each
 * LLVMFuzzerTestOneInput call, libFuzzer invokes the mutator
 * to generate the next input. The mutator reads
 * retrace_call_hash_last (exported by the retrace library via
 * LD_PRELOAD) and biases mutations:
 *
 *   - If the hash CHANGED since the last iteration, the input
 *     exercised a new libc-call sequence. Do a SMALL mutation
 *     (flip 1-2 bytes) to explore nearby inputs.
 *   - If the hash is UNCHANGED, the input hit the same call
 *     sequence. Do a LARGER mutation (insert/delete/swap) to
 *     break out of the local optimum.
 *
 * Build:
 *   CC=clang cmake -B build -DRETRACE_BUILD_FUZZERS=ON
 *   cmake --build build --target fuzz_call_hash
 *
 * Run:
 *   RETRACE_CALL_HASH=1 LD_PRELOAD=build/src/v2/libretrace.so \
 *     build/test/fuzz/fuzz_call_hash -max_total_time=60 \
 *     build/test/fuzz/corpus/call_hash
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The exported global from retrace's call_hash module.
 * Updated atomically on every intercepted libc call when
 * RETRACE_CALL_HASH=1 is set. Read here to bias mutations.
 *
 * If retrace is NOT loaded (no LD_PRELOAD), this symbol is
 * NULL/zero -- the mutator falls back to standard behavior.
 */
__attribute__((weak)) uint64_t retrace_call_hash_last;

/*
 * Track the hash from the previous iteration. If the current
 * hash differs, the input found new call-sequence coverage.
 */
static uint64_t prev_hash;
static int first_run;

/* Simple xorshift PRNG for deterministic mutations. */
static unsigned int xorshift_state = 1;

static unsigned int xorshift32(void)
{
	unsigned int x = xorshift_state;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	xorshift_state = x;
	return x;
}

/*
 * The test target: a simple parser that exercises different
 * libc calls depending on input shape. retrace intercepts
 * these calls and updates the call_hash.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char buf[256];

	if (size == 0)
		return 0;
	if (size > sizeof(buf) - 1)
		size = sizeof(buf) - 1;

	memcpy(buf, data, size);
	buf[size] = '\0';

	/* Different input shapes trigger different libc calls,
	 * producing different call_hash values. The mutator
	 * uses these differences as coverage feedback.
	 */
	if (buf[0] == 'A') {
		/* Path 1: string operations */
		strlen(buf);
	} else if (buf[0] == 'B') {
		/* Path 2: memory operations */
		memset(buf + 1, 0, size - 1);
	} else if (buf[0] == 'C') {
		/* Path 3: I/O simulation */
		snprintf(buf, sizeof(buf), "%d", (int)size);
	} else {
		/* Path 4: no calls */
	}

	return 0;
}

/*
 * Custom mutator. Called by libFuzzer after each
 * LLVMFuzzerTestOneInput to generate the next input.
 *
 * Strategy: read retrace's call_hash to decide mutation size.
 * New hash (new call sequence) → small mutation (explore nearby).
 * Same hash (same call sequence) → large mutation (break out).
 */
size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
			       size_t maxsize, unsigned int seed)
{
	uint64_t current_hash;
	int hash_changed;

	xorshift_state = seed;

	/* If retrace is not loaded, fall back to default behavior. */
	if (&retrace_call_hash_last == NULL) {
		/* Standard small mutation: flip 1 byte. */
		if (size > 0)
			data[xorshift32() % size] ^= (xorshift32() & 0xff);
		return size;
	}

	current_hash = retrace_call_hash_last;
	hash_changed = (current_hash != prev_hash);

	if (first_run) {
		first_run = 0;
		hash_changed = 1;
	}

	prev_hash = current_hash;

	if (hash_changed) {
		/*
		 * New call-sequence coverage found! Do a small
		 * mutation to explore nearby inputs that might
		 * also hit this path.
		 */
		if (size > 0) {
			int n_flips = 1 + (xorshift32() % 2);
			int i;

			for (i = 0; i < n_flips && size > 0; i++)
				data[xorshift32() % size] ^=
					(1 << (xorshift32() % 8));
		}
	} else {
		/*
		 * No new coverage. Do a larger mutation to break
		 * out of this local optimum.
		 */
		int strategy = xorshift32() % 3;

		switch (strategy) {
		case 0:
			/* Insert a random byte. */
			if (size < maxsize) {
				memmove(data + 1, data, size);
				data[0] = (uint8_t)(xorshift32() & 0xff);
				size++;
			}
			break;
		case 1:
			/* Delete the first byte. */
			if (size > 1) {
				memmove(data, data + 1, size - 1);
				size--;
			}
			break;
		case 2:
			/* Overwrite first byte with a random value. */
			if (size > 0)
				data[0] = (uint8_t)(xorshift32() & 0xff);
			break;
		}
	}

	return size;
}
