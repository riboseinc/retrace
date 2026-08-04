/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Property-based test PRNG. xorshift64 — deterministic from a seed,
 * tiny (4 lines of state), good statistical properties for testing.
 *
 * Default seed is fixed (1) so test runs are reproducible. Override
 * via the RETRACE_PROPERTY_SEED env var when chasing a specific
 * failure.
 */
#ifndef RETRACE_TEST_PROPERTY_PRNG_H
#define RETRACE_TEST_PROPERTY_PRNG_H

#include <stdint.h>

struct ret_prng {
	uint64_t state;
};

static inline void ret_prng_seed(struct ret_prng *p, uint64_t seed)
{
	p->state = seed ? seed : 0x9E3779B97F4A7C15ULL;
}

static inline uint64_t ret_prng_next(struct ret_prng *p)
{
	uint64_t x = p->state;

	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;

	p->state = x;
	return x;
}

static inline uint32_t ret_prng_u32(struct ret_prng *p, uint32_t bound_exclusive)
{
	if (bound_exclusive == 0)
		return 0;
	return (uint32_t)(ret_prng_next(p) % bound_exclusive);
}

static inline uint8_t ret_prng_byte(struct ret_prng *p)
{
	return (uint8_t)ret_prng_next(p);
}

#endif
