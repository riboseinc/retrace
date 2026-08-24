/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Retry timing policy — pure functions, extracted from exporter.c
 * (v0.6.13) so the timing invariants are property-testable without
 * driving the whole pipeline:
 *
 *   - full jitter: the drawn delay is always within
 *     [0, min(initial << (attempt-1), max)]
 *   - the exponent shift is clamped, so huge attempt values cannot
 *     overflow the exponentiation (CWE-190)
 *   - a server-requested floor (Retry-After, RFC 7231 §7.1.3) never
 *     lets the delay fall below it, yet never exceeds our own cap
 *
 * No clocks, no exporter state: everything is a function of the
 * arguments. The PRNG state is caller-owned (tick-thread only).
 */
#ifndef OTLP_RETRY_POLICY_H
#define OTLP_RETRY_POLICY_H

#include <stdbool.h>
#include <stdint.h>

struct otlp_retry_cfg
{
	uint32_t initial_ms;
	uint32_t max_ms;
};

/* One xorshift64s step (multiplied out). Deterministic per state;
 * a nonzero state never yields zero. Tick-thread only. */
uint64_t
otlp_jitter_next(uint64_t *state);

/* Full-jitter ceiling for an attempt: min(initial << (attempt-1),
 * max), with the shift count clamped at 31. */
uint32_t
otlp_retry_base_delay_ms(uint32_t attempt, const struct otlp_retry_cfg *cfg);

/* Final retry delay: a uniform draw in [0, base], never below
 * server_floor_ms, never above max_ms. *server_driven (optional
 * out-param) is set when the floor won over the drawn jitter. */
uint32_t
otlp_retry_delay_ms(uint64_t *prng,
	uint32_t attempt,
	uint32_t server_floor_ms,
	const struct otlp_retry_cfg *cfg,
	bool *server_driven);

#endif /* OTLP_RETRY_POLICY_H */
