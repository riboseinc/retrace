/* SPDX-License-Identifier: BSD-3-Clause */
/* Retry timing policy — see retry_policy.h. Logic extracted
 * verbatim from exporter.c (v0.6.13); behavior is unchanged. */
#include "retry_policy.h"

uint64_t
otlp_jitter_next(uint64_t *s)
{
	uint64_t v = *s;

	v ^= v << 13;
	v ^= v >> 7;
	v ^= v << 17;
	*s = v;
	return v * 0x2545F4914F6CDD1DULL;
}

uint32_t
otlp_retry_base_delay_ms(uint32_t attempt, const struct otlp_retry_cfg *cfg)
{
	uint64_t exp;
	uint32_t shift = attempt - 1;
	uint64_t delay;

	if (shift > 31)
		shift = 31; /* beyond any uint32 magnitude anyway */
	exp = (uint64_t) cfg->initial_ms << shift;
	delay = exp > cfg->max_ms ? cfg->max_ms : exp;
	return (uint32_t) delay;
}

uint32_t
otlp_retry_delay_ms(uint64_t *prng,
	uint32_t attempt,
	uint32_t server_floor_ms,
	const struct otlp_retry_cfg *cfg,
	bool *server_driven)
{
	uint32_t base = otlp_retry_base_delay_ms(attempt, cfg);
	uint64_t drawn;
	uint32_t delay;

	if (base == 0)
	{
		/* Degenerate config (max == 0): the only legal delay is
		 * 0; the floor cannot lift it above our own cap. */
		if (server_driven)
			*server_driven = false;
		return 0;
	}
	drawn = (uint32_t)(otlp_jitter_next(prng) % ((uint64_t) base + 1));
	if (server_driven)
		*server_driven = server_floor_ms > drawn;
	delay = (uint32_t) drawn;
	if (server_floor_ms > delay)
		delay = server_floor_ms;
	if (delay > cfg->max_ms)
		delay = cfg->max_ms;
	return delay;
}
