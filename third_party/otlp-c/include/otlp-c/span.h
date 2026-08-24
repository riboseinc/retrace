/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Span — the unit of telemetry. A span represents an operation with
 * a start time, end time, attributes, status, and optional events.
 *
 * Lifetime: caller-owned. Construct via otlp_span_create() or
 * otlp_tracer_start_span(); free via otlp_span_free().
 *
 * Thread-safety: spans are single-threaded. Each thread builds and
 * frees its own spans. The exporter is thread-safe; the span itself
 * is not.
 *
 * References: see docs/otlp-spec.md for the wire schema.
 */
#ifndef OTLP_C_SPAN_H
#define OTLP_C_SPAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "status.h"
#include "value.h"
#include "visibility.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/* Opaque handle. The struct definition lives in src/span.c. */
	typedef struct otlp_span otlp_span_t;

	/* SpanKind enum — see docs/otlp-spec.md. */
	typedef enum
	{
		OTLP_SPAN_KIND_UNSPECIFIED = 0,
		OTLP_SPAN_KIND_INTERNAL = 1,
		OTLP_SPAN_KIND_SERVER = 2,
		OTLP_SPAN_KIND_CLIENT = 3,
		OTLP_SPAN_KIND_PRODUCER = 4,
		OTLP_SPAN_KIND_CONSUMER = 5
	} otlp_span_kind_t;

	/* StatusCode enum — see docs/otlp-spec.md. */
	typedef enum
	{
		OTLP_STATUS_CODE_UNSET = 0,
		OTLP_STATUS_CODE_OK = 1,
		OTLP_STATUS_CODE_ERROR = 2
	} otlp_status_code_t;

/* Trace and span IDs are fixed-length byte buffers. */
#define OTLP_TRACE_ID_LEN 16
#define OTLP_SPAN_ID_LEN 8

	/* ── Lifecycle ───────────────────────────────────────────────────
	 */

	/* Construct a span with the given name. Caller owns the result.
	 * Returns NULL on allocation failure. */
	OTLP_C_EXPORT
	otlp_span_t *otlp_span_create(const char *name);

	/* Free a span constructed via otlp_span_create or returned from
	 * otlp_tracer_start_span. Safe to call with NULL (no-op). */
	OTLP_C_EXPORT
	void otlp_span_free(otlp_span_t *span);

	/* ── Identity ────────────────────────────────────────────────────
	 *
	 * W3C Trace Context §3.1.1/§3.1.2 forbids all-zero trace-id and
	 * parent-id. The library enforces this at set time — passing an
	 * all-zero buffer returns OTLP_ERR_INVALID_ARGUMENT so invalid IDs
	 * never reach the wire.
	 */

	/* Manually set the trace ID (16 bytes). Most callers should use
	 * otlp_tracer_start_span which auto-generates IDs.
	 *
	 * Returns OTLP_ERR_INVALID_ARGUMENT if `trace_id` is all-zero
	 * (W3C violation). */
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_trace_id(otlp_span_t *span,
		const uint8_t *trace_id);

	/* Manually set the span ID (8 bytes).
	 *
	 * Returns OTLP_ERR_INVALID_ARGUMENT if `span_id` is all-zero
	 * (W3C violation). */
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_span_id(otlp_span_t *span,
		const uint8_t *span_id);

	/* Set the parent span ID. Pass NULL to clear the parent link
	 * (makes the span a root). Pass a non-NULL 8-byte buffer to set
	 * the parent.
	 *
	 * Returns OTLP_ERR_INVALID_ARGUMENT if `parent` is non-NULL and
	 * all-zero (W3C violation). Use NULL to clear, not all-zero. */
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_parent_span_id(otlp_span_t *span,
		const uint8_t *parent);

	/* ── Timing ──────────────────────────────────────────────────────
	 */

	/* Set the start/end time. Times are nanoseconds since the Unix
	 * epoch (UTC). The library emits whatever value is set, including
	 * 0. Use otlp_span_mark_start / otlp_span_mark_end to set
	 * "now" automatically. */
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_start_time(otlp_span_t *span,
		uint64_t unix_nano);

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_end_time(otlp_span_t *span,
		uint64_t unix_nano);

	/* Convenience: set start_time to "now" and end_time to "now".
	 * Uses monotonic clock internally; converts to wall-clock for
	 * the wire format. */
	OTLP_C_EXPORT
	otlp_status_t otlp_span_mark_start(otlp_span_t *span);

	OTLP_C_EXPORT
	otlp_status_t otlp_span_mark_end(otlp_span_t *span);

	/* ── Metadata ────────────────────────────────────────────────────
	 */

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_kind(otlp_span_t *span,
		otlp_span_kind_t kind);

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_name(otlp_span_t *span, const char *name);

	/* ── Attributes ──────────────────────────────────────────────────
	 *
	 * Attributes are a map: setting an existing key replaces its
	 * value (last write wins — the OTel API / OTLP data-model
	 * semantic; the type may change). Max 128 distinct keys;
	 * overflow returns OTLP_ERR_OVERFLOW. Replacing an existing
	 * key always succeeds, even at cap.
	 *
	 * String keys and string values must be valid UTF-8 (the
	 * proto3 string contract); invalid input returns
	 * OTLP_ERR_UTF8. bytes values are exempt.
	 */

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_attribute_string(otlp_span_t *span,
		const char *key,
		const char *value);

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_attribute_int(otlp_span_t *span,
		const char *key,
		int64_t value);

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_attribute_double(otlp_span_t *span,
		const char *key,
		double value);

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_attribute_bool(otlp_span_t *span,
		const char *key,
		bool value);

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_attribute_bytes(otlp_span_t *span,
		const char *key,
		const uint8_t *bytes,
		size_t len);

	/* Set an ArrayValue attribute from `n` scalar values (the
	 * library deep-copies; the caller keeps ownership of the
	 * inputs). Same upsert semantics as the scalar setters. */
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_attribute_array(otlp_span_t *span,
		const char *key,
		const otlp_value_t *items,
		size_t n);

	/* Set a KeyValueList attribute from `n` (key, value) entries. */
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_attribute_kvlist(otlp_span_t *span,
		const char *key,
		const otlp_kv_t *entries,
		size_t n);

	/* ── Status ──────────────────────────────────────────────────────
	 */

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_status(otlp_span_t *span,
		otlp_status_code_t code,
		const char *description);

	/* ── Sampling ──────────────────────────────────────────────────
	 *
	 * The sampled flag controls the W3C trace-flags byte (bit 0).
	 * Default: true (sampled). Set to false for unsampled spans; the
	 * exporter will emit the flags field on the wire as 0x00, and
	 * the collector may drop the span.
	 *
	 * The library does NOT implement sampling policies (rate-limited,
	 * probabilistic, etc.). The caller decides which spans to sample
	 * and calls this function accordingly. */

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_sampled(otlp_span_t *span, bool sampled);

	/* Read the sampled flag. Default: true. */
	OTLP_C_EXPORT
	bool otlp_span_is_sampled(const otlp_span_t *span);

	/* ── Events / Links / TraceState ────────────────────────────────
	 *
	 * Span.Event, Span.Link, and trace_state complete the OTLP Span
	 * message. Events carry a name + timestamp (attributes deferred to
	 * a future API extension). Links reference another trace/span pair
	 * (trace_state, attributes, flags deferred). trace_state propagates
	 * vendor-specific trace context (W3C tracestate header).
	 *
	 * Fixed-cap storage: max 64 events, 64 links per span. Overflow
	 * returns OTLP_ERR_OVERFLOW. */

	OTLP_C_EXPORT
	otlp_status_t otlp_span_add_event(otlp_span_t *span,
		const char *name,
		uint64_t time_unix_nano);

	/* Set an attribute on the most-recently-added event. Call after
	 * otlp_span_add_event. Up to 32 attributes per event. Same type
	 * set as the span-level attribute setters. */
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_event_attribute_string(otlp_span_t *span,
		const char *key,
		const char *value);
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_event_attribute_int(otlp_span_t *span,
		const char *key,
		int64_t value);
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_event_attribute_double(otlp_span_t *span,
		const char *key,
		double value);
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_event_attribute_bool(otlp_span_t *span,
		const char *key,
		bool value);
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_event_attribute_bytes(otlp_span_t *span,
		const char *key,
		const uint8_t *bytes,
		size_t len);

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_event_attribute_array(otlp_span_t *span,
		const char *key,
		const otlp_value_t *items,
		size_t n);
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_event_attribute_kvlist(otlp_span_t *span,
		const char *key,
		const otlp_kv_t *entries,
		size_t n);

	OTLP_C_EXPORT
	otlp_status_t otlp_span_add_link(otlp_span_t *span,
		const uint8_t *trace_id,
		const uint8_t *span_id);

	/* Set an attribute on the most-recently-added link. Call after
	 * otlp_span_add_link. Up to 32 attributes per link. Same type
	 * set as the span-level attribute setters. */
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_link_attribute_string(otlp_span_t *span,
		const char *key,
		const char *value);
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_link_attribute_int(otlp_span_t *span,
		const char *key,
		int64_t value);
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_link_attribute_double(otlp_span_t *span,
		const char *key,
		double value);
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_link_attribute_bool(otlp_span_t *span,
		const char *key,
		bool value);
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_link_attribute_bytes(otlp_span_t *span,
		const char *key,
		const uint8_t *bytes,
		size_t len);

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_link_attribute_array(otlp_span_t *span,
		const char *key,
		const otlp_value_t *items,
		size_t n);
	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_link_attribute_kvlist(otlp_span_t *span,
		const char *key,
		const otlp_kv_t *entries,
		size_t n);

	OTLP_C_EXPORT
	otlp_status_t otlp_span_set_trace_state(otlp_span_t *span,
		const char *trace_state);

#ifdef __cplusplus
}
#endif

#endif
