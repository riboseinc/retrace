/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Sampler — pluggable sampling decision policy.
 *
 * A sampler decides whether a new span should be recorded. The
 * decision is made at span-creation time (head sampling), before
 * any attributes or events are added. This is the cheapest place
 * to drop a span: no work is done on dropped spans.
 *
 * Built-in samplers:
 *   - always_on               sample 100%
 *   - always_off              sample 0%
 *   - trace_id_ratio_based    deterministic ratio based on trace_id
 *                             (the same trace_id always yields the
 *                             same decision, so downstream services
 *                             agree on sampling)
 *
 * Custom samplers implement the otlp_sampler_t vtable. The library
 * calls should_sample() at start_span time and respects the decision.
 *
 * Thread-safety: samplers must be safe to call from multiple
 * threads. The built-in samplers are stateless (read-only after
 * create) so this is automatic.
 */
#ifndef OTLP_C_SAMPLER_H
#define OTLP_C_SAMPLER_H

#include <otlp-c/span.h>
#include <otlp-c/visibility.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct otlp_sampler otlp_sampler_t;

	typedef enum
	{
		/* Drop the span entirely. The library frees the in-progress
		 * span and returns NULL from start_span. */
		OTLP_SAMPLING_DECISION_NOT_RECORD = 0,
		/* Record the span but mark it as not sampled. Downstream
		 * collectors may aggregate or drop these. */
		OTLP_SAMPLING_DECISION_RECORD = 1,
		/* Record and mark as sampled (the typical "yes" decision). */
		OTLP_SAMPLING_DECISION_RECORD_AND_SAMPLED = 2,
	} otlp_sampling_decision_t;

	typedef struct
	{
		otlp_sampling_decision_t decision;
	} otlp_sampling_result_t;

	/* vtable — custom samplers populate this. */
	typedef otlp_sampling_result_t (*otlp_sampler_should_sample_fn)(
		const otlp_sampler_t *sampler,
		const uint8_t trace_id[16],
		const char *name,
		otlp_span_kind_t kind);

	typedef void (*otlp_sampler_free_fn)(otlp_sampler_t *sampler);

	struct otlp_sampler
	{
		otlp_sampler_should_sample_fn should_sample;
		otlp_sampler_free_fn free;
	};

	/* Construct a sampler that always returns RECORD_AND_SAMPLED. */
	OTLP_C_EXPORT
	otlp_sampler_t *otlp_sampler_always_on(void);

	/* Construct a sampler that always returns NOT_RECORD.
	 * Returns NULL on allocation failure. */
	OTLP_C_EXPORT
	otlp_sampler_t *otlp_sampler_always_off(void);

	/* Construct a deterministic ratio-based sampler. `ratio` is clamped
	 * to [0, 1]. Uses the first 8 bytes of trace_id as a uint64 and
	 * thresholds against (ratio * UINT64_MAX). The same trace_id always
	 * yields the same decision. Returns NULL on allocation failure. */
	OTLP_C_EXPORT
	otlp_sampler_t *otlp_sampler_trace_id_ratio_based(double ratio);

	/* Free a sampler. Safe to call with NULL (no-op). */
	OTLP_C_EXPORT
	void otlp_sampler_free(otlp_sampler_t *sampler);

#ifdef __cplusplus
}
#endif

#endif
