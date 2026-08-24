/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Tracer — owns span context (trace ID propagation, instrumentation
 * scope). The typical caller uses one tracer per process, named
 * after the service or library emitting telemetry.
 *
 * Lifetime: caller-owned. Construct via otlp_tracer_create(); free
 * via otlp_tracer_free().
 *
 * Thread-safety: tracer_t is safe to share across threads for the
 * purposes of start_span. Each span returned is single-threaded.
 *
 * The tracer does NOT emit spans; that's the exporter's job. The
 * typical workflow is: tracer.start_span(...) -> mutate -> exporter.
 * emit(span).
 */
#ifndef OTLP_C_TRACER_H
#define OTLP_C_TRACER_H

#include <stddef.h>
#include <stdint.h>

#include "sampler.h"
#include "span.h"
#include "status.h"
#include "visibility.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct otlp_tracer otlp_tracer_t;

/* Construct a tracer. `service_name` is the value of the
 * `service.name` resource attribute, attached to every span this
 * tracer emits. `scope_name` and `scope_version` identify the
 * instrumentation library (typically the calling library's name
 * and version, e.g. "myapp" / "1.0.0").
 *
 * Any of the strings may be NULL (treated as empty). The strings
 * are copied; the caller may free them after this call returns.
 *
 * Returns NULL on allocation failure.
 */
OTLP_C_EXPORT
otlp_tracer_t *otlp_tracer_create(const char *service_name,
				  const char *scope_name,
				  const char *scope_version);

OTLP_C_EXPORT
void otlp_tracer_free(otlp_tracer_t *tracer);

/* Set the tracer's sampler. The tracer consults the sampler at
 * start_span time. NOT_RECORD means start_span returns NULL.
 * RECORD_AND_SAMPLED (default) sets the span's sampled flag.
 * RECORD clears it.
 *
 * The tracer does NOT take ownership of `sampler` if a previous
 * sampler was set; the caller must free the previous one first.
 * Pass NULL to revert to the default (always_on). */
OTLP_C_EXPORT
void otlp_tracer_set_sampler(otlp_tracer_t *tracer,
			     otlp_sampler_t *sampler);

/* Start a span. The tracer:
 *   - generates a random trace ID and span ID
 *   - sets start_time to "now"
 *   - sets kind = INTERNAL
 *
 * The returned span is owned by the caller; free with
 * otlp_span_free.
 *
 * Returns NULL on allocation failure.
 */
OTLP_C_EXPORT
otlp_span_t *otlp_tracer_start_span(otlp_tracer_t *tracer,
				    const char *name);

/* Start a child span. Same as start_span, but with the parent's
 * trace ID and a parent_span_id linking the new span to the
 * parent. */
OTLP_C_EXPORT
otlp_span_t *otlp_tracer_start_child_span(otlp_tracer_t *tracer,
					  const char *name,
					  const otlp_span_t *parent);

#ifdef __cplusplus
}
#endif

#endif
