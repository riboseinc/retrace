/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * OTLP message encoders — turn public types into protobuf wire bytes.
 *
 * Design (see plan D1): no parallel C-struct layer mirroring the
 * .proto. The encoder walks the public accessors and emits wire
 * bytes directly. Field numbers live in src/otlp_schema.h (single
 * source of truth) or in per-signal #defines grouped by message.
 *
 * Sub-message pattern: each emit_X function takes a parent buf and
 * a field number, allocates a temp buf for the sub-message body,
 * encodes into it, wraps it via otlp_pb_field_message, and frees
 * the temp buf.
 *
 * The shared helpers (otlp_encode_any_value, otlp_emit_resource,
 * otlp_emit_instrumentation_scope) are exposed so the metrics and
 * logs encoders can compose them without duplicating the resource
 * envelope or AnyValue dispatch logic.
 */
#ifndef OTLP_C_OTLP_MESSAGES_H
#define OTLP_C_OTLP_MESSAGES_H

#include "protobuf_encode.h"
#include "span_internal.h"

struct otlp_attribute;

#include <otlp-c/exporter.h>
#include <otlp-c/log.h>
#include <otlp-c/metric.h>

#include <stddef.h>

/* ── Shared helpers (used by traces / metrics / logs encoders) ──── */

/* Encode an AnyValue oneof into `buf`. Dispatch is table-driven via
 * attr_encoders[] (see otlp_messages.c); adding a new variant is
 * one table entry, not a new switch case. */
otlp_status_t
otlp_encode_any_value(struct otlp_pb_buf *buf,
	const struct otlp_attribute *attr);

/* Encode a KeyValue (key + AnyValue oneof) into `out`. Exposed for
 * tests and for encoders that need to emit resource/instrumentation
 * attributes without going through the top-level request encoder. */
otlp_status_t
otlp_encode_key_value(struct otlp_pb_buf *out,
	const char *key,
	const struct otlp_attribute *attr);

/* Emit a Resource sub-message at `field_num` on `parent`. The
 * Resource carries:
 *   - service.name (from `service_name`; skipped if NULL/empty)
 *   - any additional attributes from `attrs` (e.g. service.version,
 *     deployment.environment, host.name). May be NULL/0.
 *
 * No-op (returns OTLP_OK without emitting) if service_name is
 * NULL/empty AND attrs is NULL/empty — an empty Resource is not
 * emitted, matching the protobuf convention. */
otlp_status_t
otlp_emit_resource(struct otlp_pb_buf *parent,
	uint32_t field_num,
	const char *service_name,
	const struct otlp_attribute *attrs,
	size_t n_attrs);

/* Emit an InstrumentationScope sub-message at `field_num` on
 * `parent`. No-op if both name and version are NULL/empty. */
otlp_status_t
otlp_emit_instrumentation_scope(struct otlp_pb_buf *parent,
	uint32_t field_num,
	const char *name,
	const char *version);

/* Emit a repeated KeyValue list at `field_num` on `parent`. Each
 * attribute is encoded via otlp_encode_key_value. Shared by the
 * traces / metrics / logs encoders for the repeated attributes
 * field (Resource, Span, NumberDataPoint, HistogramDataPoint,
 * Event, Link, LogRecord). DRY. */
otlp_status_t
otlp_emit_attributes(struct otlp_pb_buf *parent,
	uint32_t field_num,
	const struct otlp_attribute *attrs,
	size_t n_attrs);

/* ── Traces ────────────────────────────────────────────────────── */

/* Encode a complete ExportTraceServiceRequest body into `out`.
 *
 * `service_name` becomes the value of the resource attribute
 * "service.name" (skipped if NULL or empty).
 * `resource_attributes` are emitted as additional Resource attributes
 * after service.name (may be NULL when n_resource_attributes is 0).
 * `scope_name` / `scope_version` populate InstrumentationScope
 * (skipped if both NULL or empty).
 * `spans` is an array of `n_spans` span pointers.
 *
 * Empty request (no service name, no attrs, no scope, no spans)
 * produces a zero-length body, matching the spec. */
otlp_status_t
otlp_encode_export_trace_service_request(struct otlp_pb_buf *out,
	const char *service_name,
	const struct otlp_attribute *resource_attributes,
	size_t n_resource_attributes,
	const char *scope_name,
	const char *scope_version,
	const otlp_span_t *const *spans,
	size_t n_spans);

/* Encode a single Span body into `out` (no ResourceSpans wrapping). */
otlp_status_t
otlp_encode_span_body(struct otlp_pb_buf *out, const otlp_span_t *span);

/* ── Metrics ───────────────────────────────────────────────────── */

otlp_status_t
otlp_encode_export_metrics_service_request(struct otlp_pb_buf *out,
	const char *service_name,
	const struct otlp_attribute *resource_attributes,
	size_t n_resource_attributes,
	const char *scope_name,
	const char *scope_version,
	const otlp_metric_t *const *metrics,
	size_t n_metrics);

/* ── Logs ──────────────────────────────────────────────────────── */

otlp_status_t
otlp_encode_export_logs_service_request(struct otlp_pb_buf *out,
	const char *service_name,
	const struct otlp_attribute *resource_attributes,
	size_t n_resource_attributes,
	const char *scope_name,
	const char *scope_version,
	const otlp_log_record_t *const *logs,
	size_t n_logs);

#endif
