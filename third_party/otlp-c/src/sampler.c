/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Sampler implementations. See include/otlp-c/sampler.h.
 *
 * The built-in samplers are stateless after construction — the
 * should_sample() function reads only its arguments and the
 * (immutable) ratio / decision configuration. This makes them
 * safe to call from multiple threads without synchronization.
 */
#include <otlp-c/sampler.h>
#include <otlp-c/span.h>

#include "internal_util.h"

#include <stdint.h>
#include <string.h>

/* ── AlwaysOn ─────────────────────────────────────────────────── */

static otlp_sampling_result_t
always_on_should_sample(const otlp_sampler_t *sampler,
	const uint8_t trace_id[16],
	const char *name,
	otlp_span_kind_t kind)
{
	otlp_sampling_result_t r;

	(void) sampler;
	(void) trace_id;
	(void) name;
	(void) kind;
	r.decision = OTLP_SAMPLING_DECISION_RECORD_AND_SAMPLED;
	return r;
}

otlp_sampler_t *
otlp_sampler_always_on(void)
{
	static otlp_sampler_t static_always_on = {
		.should_sample = always_on_should_sample,
		.free = NULL,
	};
	return &static_always_on;
}

/* ── AlwaysOff ────────────────────────────────────────────────── */

static otlp_sampling_result_t
always_off_should_sample(const otlp_sampler_t *sampler,
	const uint8_t trace_id[16],
	const char *name,
	otlp_span_kind_t kind)
{
	otlp_sampling_result_t r;

	(void) sampler;
	(void) trace_id;
	(void) name;
	(void) kind;
	r.decision = OTLP_SAMPLING_DECISION_NOT_RECORD;
	return r;
}

otlp_sampler_t *
otlp_sampler_always_off(void)
{
	static otlp_sampler_t static_always_off = {
		.should_sample = always_off_should_sample,
		.free = NULL,
	};
	return &static_always_off;
}

/* ── TraceIdRatioBased ────────────────────────────────────────── */

struct ratio_sampler
{
	otlp_sampler_t base;
	double ratio;
};

static otlp_sampling_result_t
ratio_should_sample(const otlp_sampler_t *sampler,
	const uint8_t trace_id[16],
	const char *name,
	otlp_span_kind_t kind)
{
	const struct ratio_sampler *rs = (const struct ratio_sampler *) sampler;
	otlp_sampling_result_t r;
	uint64_t trace_prefix;

	(void) name;
	(void) kind;
	/* Endpoints: ratio <= 0 never samples, ratio >= 1 always samples.
	 * These must be exact because the formula below has off-by-one
	 * edge effects at the boundary (e.g., trace_prefix == UINT64_MAX
	 * with ratio = 1.0 would not sample under pure integer compare). */
	if (rs->ratio <= 0.0)
	{
		r.decision = OTLP_SAMPLING_DECISION_NOT_RECORD;
		return r;
	}
	if (rs->ratio >= 1.0)
	{
		r.decision = OTLP_SAMPLING_DECISION_RECORD_AND_SAMPLED;
		return r;
	}
	/* Use the first 8 bytes of trace_id (big-endian, matching
	 * otel-go's binary.BigEndian.Uint64(traceID[0:8])) as the
	 * deterministic threshold input. Same trace_id → same
	 * decision on every platform. (A native memcpy would give a
	 * byte-reversed — i.e. platform-dependent — value on
	 * little-endian.) */
	trace_prefix = ((uint64_t) trace_id[0] << 56) |
		((uint64_t) trace_id[1] << 48) |
		((uint64_t) trace_id[2] << 40) |
		((uint64_t) trace_id[3] << 32) |
		((uint64_t) trace_id[4] << 24) |
		((uint64_t) trace_id[5] << 16) | ((uint64_t) trace_id[6] << 8) |
		(uint64_t) trace_id[7];
	/* OTel-spec-suggested formula: threshold = ratio * UINT64_MAX.
	 * Integer comparison matches the otel-cpp / otel-java / otel-go
	 * samplers for cross-SDK trace consistency at the boundary. */
	{
		uint64_t threshold =
			(uint64_t)(rs->ratio * (double) UINT64_MAX);

		if (trace_prefix < threshold)
			r.decision = OTLP_SAMPLING_DECISION_RECORD_AND_SAMPLED;
		else
			r.decision = OTLP_SAMPLING_DECISION_NOT_RECORD;
	}
	return r;
}

static void
ratio_free(otlp_sampler_t *sampler)
{
	otlp_free(sampler);
}

otlp_sampler_t *
otlp_sampler_trace_id_ratio_based(double ratio)
{
	struct ratio_sampler *rs;

	if (ratio < 0.0)
		ratio = 0.0;
	if (ratio > 1.0)
		ratio = 1.0;
	rs = otlp_malloc(sizeof(*rs));
	if (!rs)
		return NULL;
	rs->base.should_sample = ratio_should_sample;
	rs->base.free = ratio_free;
	rs->ratio = ratio;
	return &rs->base;
}

void
otlp_sampler_free(otlp_sampler_t *sampler)
{
	if (!sampler)
		return;
	if (sampler->free)
		sampler->free(sampler);
}
