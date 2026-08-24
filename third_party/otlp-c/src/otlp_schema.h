/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * OTLP schema tables — model-driven field-number / wire-type matrix
 * per OTLP message. Single source of truth; the encoders in
 * otlp_messages.c reference these constants instead of local #defines.
 *
 * Source: docs/otlp-spec.md (which mirrors opentelemetry-proto).
 *
 * Design: each message has a named-enum index + designated-initializer
 * table. Adding or reordering fields is safe — the enum ensures the
 * accessor macros (in otlp_messages.c) always reference the right
 * entry regardless of array order.
 */
#ifndef OTLP_C_OTLP_SCHEMA_H
#define OTLP_C_OTLP_SCHEMA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protobuf_encode.h"

/* ── Types ────────────────────────────────────────────────────── */

enum otlp_field_presence
{
	OTLP_PRESENCE_DEFAULT_OMITTED = 0,
	OTLP_PRESENCE_ALWAYS_EMIT,
};

struct otlp_field_spec
{
	const char *name;
	uint32_t number;
	int wire_type;
	enum otlp_field_presence presence;
	bool repeated;
};

/* ── ExportTraceServiceRequest ────────────────────────────────── */

enum
{
	OTLP_ETSR_FI_RESOURCE_SPANS,
	OTLP_ETSR_FI_COUNT,
};

static const struct otlp_field_spec OTLP_ETSR_FIELDS[] = {
	[OTLP_ETSR_FI_RESOURCE_SPANS] = { "resource_spans",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
};

/* ── ResourceSpans ────────────────────────────────────────────── */

enum
{
	OTLP_RS_FI_RESOURCE,
	OTLP_RS_FI_SCOPE_SPANS,
	OTLP_RS_FI_SCHEMA_URL,
	OTLP_RS_FI_COUNT,
};

static const struct otlp_field_spec OTLP_RS_FIELDS[] = {
	[OTLP_RS_FI_RESOURCE] = { "resource",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_RS_FI_SCOPE_SPANS] = { "scope_spans",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_RS_FI_SCHEMA_URL] = { "schema_url",
		3,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── Resource ─────────────────────────────────────────────────── */

enum
{
	OTLP_R_FI_ATTRIBUTES,
	OTLP_R_FI_DROPPED_ATTRS,
	OTLP_R_FI_COUNT,
};

static const struct otlp_field_spec OTLP_R_FIELDS[] = {
	[OTLP_R_FI_ATTRIBUTES] = { "attributes",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_R_FI_DROPPED_ATTRS] = { "dropped_attributes_count",
		2,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── ScopeSpans ───────────────────────────────────────────────── */

enum
{
	OTLP_SS_FI_SCOPE,
	OTLP_SS_FI_SPANS,
	OTLP_SS_FI_SCHEMA_URL,
	OTLP_SS_FI_COUNT,
};

static const struct otlp_field_spec OTLP_SS_FIELDS[] = {
	[OTLP_SS_FI_SCOPE] = { "scope",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_SS_FI_SPANS] = { "spans",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_SS_FI_SCHEMA_URL] = { "schema_url",
		3,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── InstrumentationScope ─────────────────────────────────────── */

enum
{
	OTLP_IS_FI_NAME,
	OTLP_IS_FI_VERSION,
	OTLP_IS_FI_ATTRIBUTES,
	OTLP_IS_FI_DROPPED_ATTRS,
	OTLP_IS_FI_COUNT,
};

static const struct otlp_field_spec OTLP_IS_FIELDS[] = {
	[OTLP_IS_FI_NAME] = { "name",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_IS_FI_VERSION] = { "version",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_IS_FI_ATTRIBUTES] = { "attributes",
		3,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_IS_FI_DROPPED_ATTRS] = { "dropped_attributes_count",
		4,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── Span ─────────────────────────────────────────────────────── */

enum
{
	OTLP_SPAN_FI_TRACE_ID,
	OTLP_SPAN_FI_SPAN_ID,
	OTLP_SPAN_FI_TRACE_STATE,
	OTLP_SPAN_FI_PARENT_SPAN_ID,
	OTLP_SPAN_FI_NAME,
	OTLP_SPAN_FI_KIND,
	OTLP_SPAN_FI_START_TIME,
	OTLP_SPAN_FI_END_TIME,
	OTLP_SPAN_FI_ATTRIBUTES,
	OTLP_SPAN_FI_DROPPED_ATTRS,
	OTLP_SPAN_FI_EVENTS,
	OTLP_SPAN_FI_DROPPED_EVENTS,
	OTLP_SPAN_FI_LINKS,
	OTLP_SPAN_FI_DROPPED_LINKS,
	OTLP_SPAN_FI_STATUS,
	OTLP_SPAN_FI_FLAGS,
	OTLP_SPAN_FI_COUNT,
};

static const struct otlp_field_spec OTLP_SPAN_FIELDS[] = {
	[OTLP_SPAN_FI_TRACE_ID] = { "trace_id",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_SPAN_FI_SPAN_ID] = { "span_id",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_SPAN_FI_TRACE_STATE] = { "trace_state",
		3,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_SPAN_FI_PARENT_SPAN_ID] = { "parent_span_id",
		4,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_SPAN_FI_NAME] = { "name",
		5,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_SPAN_FI_KIND] = { "kind",
		6,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_SPAN_FI_START_TIME] = { "start_time_unix_nano",
		7,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_SPAN_FI_END_TIME] = { "end_time_unix_nano",
		8,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_SPAN_FI_ATTRIBUTES] = { "attributes",
		9,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_SPAN_FI_DROPPED_ATTRS] = { "dropped_attributes_count",
		10,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_SPAN_FI_EVENTS] = { "events",
		11,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_SPAN_FI_DROPPED_EVENTS] = { "dropped_events_count",
		12,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_SPAN_FI_LINKS] = { "links",
		13,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_SPAN_FI_DROPPED_LINKS] = { "dropped_links_count",
		14,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_SPAN_FI_STATUS] = { "status",
		15,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_SPAN_FI_FLAGS] = { "flags",
		16,
		OTLP_PB_WIRE_FIXED32,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── Status ───────────────────────────────────────────────────── */

enum
{
	OTLP_STATUS_FI_MESSAGE,
	OTLP_STATUS_FI_CODE,
	OTLP_STATUS_FI_COUNT,
};

static const struct otlp_field_spec OTLP_STATUS_FIELDS[] = {
	/* Field 1 is reserved in opentelemetry-proto (deprecated code enum). */
	[OTLP_STATUS_FI_MESSAGE] = { "message",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_STATUS_FI_CODE] = { "code",
		3,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── KeyValue ─────────────────────────────────────────────────── */

enum
{
	OTLP_KV_FI_KEY,
	OTLP_KV_FI_VALUE,
	OTLP_KV_FI_COUNT,
};

static const struct otlp_field_spec OTLP_KV_FIELDS[] = {
	[OTLP_KV_FI_KEY] = { "key",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_KV_FI_VALUE] = { "value",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
};

/* ── AnyValue oneof variants ──────────────────────────────────── */

enum
{
	OTLP_AV_FI_STRING,
	OTLP_AV_FI_BOOL,
	OTLP_AV_FI_INT64,
	OTLP_AV_FI_DOUBLE,
	OTLP_AV_FI_ARRAY_VALUE,
	OTLP_AV_FI_KVLIST_VALUE,
	OTLP_AV_FI_BYTES,
	OTLP_AV_FI_COUNT,
};

static const struct otlp_field_spec OTLP_AV_FIELDS[] = {
	[OTLP_AV_FI_STRING] = { "string_value",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_AV_FI_BOOL] = { "bool_value",
		2,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_AV_FI_INT64] = { "int_value",
		3,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_AV_FI_DOUBLE] = { "double_value",
		4,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_AV_FI_ARRAY_VALUE] = { "array_value",
		5,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_AV_FI_KVLIST_VALUE] = { "kvlist_value",
		6,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_AV_FI_BYTES] = { "bytes_value",
		7,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
};

/* ── ArrayValue / KeyValueList (nested AnyValue variants) ─────── */

enum
{
	OTLP_AV_ARRAY_FI_VALUES,
	OTLP_AV_ARRAY_FI_COUNT,
};

static const struct otlp_field_spec OTLP_AV_ARRAY_FIELDS[] = {
	[OTLP_AV_ARRAY_FI_VALUES] = { "values",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
};

enum
{
	OTLP_KVLIST_FI_VALUES,
	OTLP_KVLIST_FI_COUNT,
};

static const struct otlp_field_spec OTLP_KVLIST_FIELDS[] = {
	[OTLP_KVLIST_FI_VALUES] = { "values",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
};

/* ── Span.Event ───────────────────────────────────────────────── */

enum
{
	OTLP_EVENT_FI_TIME,
	OTLP_EVENT_FI_NAME,
	OTLP_EVENT_FI_ATTRIBUTES,
	OTLP_EVENT_FI_DROPPED_ATTRS,
	OTLP_EVENT_FI_COUNT,
};

static const struct otlp_field_spec OTLP_EVENT_FIELDS[] = {
	[OTLP_EVENT_FI_TIME] = { "time_unix_nano",
		1,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_EVENT_FI_NAME] = { "name",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_EVENT_FI_ATTRIBUTES] = { "attributes",
		3,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_EVENT_FI_DROPPED_ATTRS] = { "dropped_attributes_count",
		4,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── Span.Link ────────────────────────────────────────────────── */

enum
{
	OTLP_LINK_FI_TRACE_ID,
	OTLP_LINK_FI_SPAN_ID,
	OTLP_LINK_FI_TRACE_STATE,
	OTLP_LINK_FI_ATTRIBUTES,
	OTLP_LINK_FI_DROPPED_ATTRS,
	OTLP_LINK_FI_FLAGS,
	OTLP_LINK_FI_COUNT,
};

static const struct otlp_field_spec OTLP_LINK_FIELDS[] = {
	[OTLP_LINK_FI_TRACE_ID] = { "trace_id",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_LINK_FI_SPAN_ID] = { "span_id",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_LINK_FI_TRACE_STATE] = { "trace_state",
		3,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_LINK_FI_ATTRIBUTES] = { "attributes",
		4,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_LINK_FI_DROPPED_ATTRS] = { "dropped_attributes_count",
		5,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_LINK_FI_FLAGS] = { "flags",
		6,
		OTLP_PB_WIRE_FIXED32,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ════════════════════════════════════════════════════════════════
 * Metrics signal — opentelemetry-proto metrics/v1
 * ════════════════════════════════════════════════════════════════ */

/* ── ExportMetricsServiceRequest ──────────────────────────────── */

enum
{
	OTLP_EMSR_FI_RESOURCE_METRICS,
	OTLP_EMSR_FI_COUNT,
};

static const struct otlp_field_spec OTLP_EMSR_FIELDS[] = {
	[OTLP_EMSR_FI_RESOURCE_METRICS] = { "resource_metrics",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
};

/* ── ResourceMetrics ──────────────────────────────────────────── */

enum
{
	OTLP_RM_FI_RESOURCE,
	OTLP_RM_FI_SCOPE_METRICS,
	OTLP_RM_FI_SCHEMA_URL,
	OTLP_RM_FI_COUNT,
};

static const struct otlp_field_spec OTLP_RM_FIELDS[] = {
	[OTLP_RM_FI_RESOURCE] = { "resource",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_RM_FI_SCOPE_METRICS] = { "scope_metrics",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_RM_FI_SCHEMA_URL] = { "schema_url",
		3,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── ScopeMetrics ─────────────────────────────────────────────── */

enum
{
	OTLP_SM_FI_SCOPE,
	OTLP_SM_FI_METRICS,
	OTLP_SM_FI_SCHEMA_URL,
	OTLP_SM_FI_COUNT,
};

static const struct otlp_field_spec OTLP_SM_FIELDS[] = {
	[OTLP_SM_FI_SCOPE] = { "scope",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_SM_FI_METRICS] = { "metrics",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_SM_FI_SCHEMA_URL] = { "schema_url",
		3,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── Metric (oneof data: Gauge=5, Sum=7, Histogram=9, ...) ────── */

enum
{
	OTLP_METRIC_FI_NAME,
	OTLP_METRIC_FI_DESCRIPTION,
	OTLP_METRIC_FI_UNIT,
	OTLP_METRIC_FI_GAUGE,
	OTLP_METRIC_FI_SUM,
	OTLP_METRIC_FI_HISTOGRAM,
	OTLP_METRIC_FI_EXP_HISTOGRAM,
	OTLP_METRIC_FI_COUNT,
};

static const struct otlp_field_spec OTLP_METRIC_FIELDS[] = {
	[OTLP_METRIC_FI_NAME] = { "name",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_METRIC_FI_DESCRIPTION] = { "description",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_METRIC_FI_UNIT] = { "unit",
		3,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	/* [4] reserved in proto */
	[OTLP_METRIC_FI_GAUGE] = { "gauge",
		5,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	/* [6] reserved in proto */
	[OTLP_METRIC_FI_SUM] = { "sum",
		7,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	/* [8] reserved in proto */
	[OTLP_METRIC_FI_HISTOGRAM] = { "histogram",
		9,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_METRIC_FI_EXP_HISTOGRAM] = { "exponential_histogram",
		10,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── Sum ──────────────────────────────────────────────────────── */

enum
{
	OTLP_SUM_FI_DATA_POINTS,
	OTLP_SUM_FI_AGG_TEMP,
	OTLP_SUM_FI_IS_MONOTONIC,
	OTLP_SUM_FI_COUNT,
};

static const struct otlp_field_spec OTLP_SUM_FIELDS[] = {
	[OTLP_SUM_FI_DATA_POINTS] = { "data_points",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_SUM_FI_AGG_TEMP] = { "aggregation_temporality",
		2,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_SUM_FI_IS_MONOTONIC] = { "is_monotonic",
		3,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── Gauge ────────────────────────────────────────────────────── */

enum
{
	OTLP_GAUGE_FI_DATA_POINTS,
	OTLP_GAUGE_FI_COUNT,
};

static const struct otlp_field_spec OTLP_GAUGE_FIELDS[] = {
	[OTLP_GAUGE_FI_DATA_POINTS] = { "data_points",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
};

/* ── Histogram ────────────────────────────────────────────────── */

enum
{
	OTLP_HIST_FI_DATA_POINTS,
	OTLP_HIST_FI_AGG_TEMP,
	OTLP_HIST_FI_COUNT,
};

static const struct otlp_field_spec OTLP_HIST_FIELDS[] = {
	[OTLP_HIST_FI_DATA_POINTS] = { "data_points",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_HIST_FI_AGG_TEMP] = { "aggregation_temporality",
		2,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── ExponentialHistogram ────────────────────────────────────── */

enum
{
	OTLP_EH_FI_DATA_POINTS,
	OTLP_EH_FI_AGG_TEMP,
	OTLP_EH_FI_COUNT,
};

static const struct otlp_field_spec OTLP_EH_FIELDS[] = {
	[OTLP_EH_FI_DATA_POINTS] = { "data_points",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_EH_FI_AGG_TEMP] = { "aggregation_temporality",
		2,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── NumberDataPoint (oneof value: as_double=4, as_int=6) ─────── */

enum
{
	OTLP_NDP_FI_START_TIME,
	OTLP_NDP_FI_TIME,
	OTLP_NDP_FI_AS_DOUBLE,
	OTLP_NDP_FI_ATTRIBUTES,
	OTLP_NDP_FI_COUNT,
};

static const struct otlp_field_spec OTLP_NDP_FIELDS[] = {
	/* Field 1 is reserved in opentelemetry-proto. */
	[OTLP_NDP_FI_START_TIME] = { "start_time_unix_nano",
		2,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_NDP_FI_TIME] = { "time_unix_nano",
		3,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_NDP_FI_AS_DOUBLE] = { "as_double",
		4,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	/* Fields 5 (exemplars), 6 (as_int), 8 (flags) not emitted. */
	[OTLP_NDP_FI_ATTRIBUTES] = { "attributes",
		7,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
};

/* ── HistogramDataPoint ─────────────────────────────────────────
 * Note: the proto field at index 3 is named `count`, which collides
 * with the conventional `OTLP_*_FI_COUNT` sentinel. The sentinel is
 * therefore renamed `OTLP_HDP_FI_NFIELDS` for this table only. */

enum
{
	OTLP_HDP_FI_START_TIME,
	OTLP_HDP_FI_TIME,
	OTLP_HDP_FI_COUNT,
	OTLP_HDP_FI_SUM,
	OTLP_HDP_FI_BUCKET_COUNTS,
	OTLP_HDP_FI_EXPLICIT_BOUNDS,
	OTLP_HDP_FI_ATTRIBUTES,
	OTLP_HDP_FI_MIN,
	OTLP_HDP_FI_MAX,
	OTLP_HDP_FI_NFIELDS,
};

static const struct otlp_field_spec OTLP_HDP_FIELDS[] = {
	/* Field 1 is reserved in opentelemetry-proto. */
	[OTLP_HDP_FI_START_TIME] = { "start_time_unix_nano",
		2,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_HDP_FI_TIME] = { "time_unix_nano",
		3,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_HDP_FI_COUNT] = { "count",
		4,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_HDP_FI_SUM] = { "sum",
		5,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_HDP_FI_BUCKET_COUNTS] = { "bucket_counts",
		6,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_HDP_FI_EXPLICIT_BOUNDS] = { "explicit_bounds",
		7,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	/* Fields 8 (exemplars) and 10 (flags, uint32 varint) not
	 * emitted. */
	[OTLP_HDP_FI_ATTRIBUTES] = { "attributes",
		9,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_HDP_FI_MIN] = { "min",
		11,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_HDP_FI_MAX] = { "max",
		12,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── ExponentialHistogramDataPoint ────────────────────────────── */

enum
{
	OTLP_EHDP_FI_ATTRIBUTES,
	OTLP_EHDP_FI_START_TIME,
	OTLP_EHDP_FI_TIME,
	OTLP_EHDP_FI_COUNT,
	OTLP_EHDP_FI_SUM,
	OTLP_EHDP_FI_SCALE,
	OTLP_EHDP_FI_ZERO_COUNT,
	OTLP_EHDP_FI_POSITIVE,
	OTLP_EHDP_FI_NEGATIVE,
	OTLP_EHDP_FI_FLAGS,
	OTLP_EHDP_FI_NFIELDS,
};

static const struct otlp_field_spec OTLP_EHDP_FIELDS[] = {
	[OTLP_EHDP_FI_ATTRIBUTES] = { "attributes",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_EHDP_FI_START_TIME] = { "start_time_unix_nano",
		2,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_EHDP_FI_TIME] = { "time_unix_nano",
		3,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_EHDP_FI_COUNT] = { "count",
		4,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_ALWAYS_EMIT,
		false },
	[OTLP_EHDP_FI_SUM] = { "sum",
		5,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_EHDP_FI_SCALE] = { "scale",
		6,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_EHDP_FI_ZERO_COUNT] = { "zero_count",
		7,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_EHDP_FI_POSITIVE] = { "positive",
		8,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_EHDP_FI_NEGATIVE] = { "negative",
		9,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_EHDP_FI_FLAGS] = { "flags",
		10,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── ExponentialHistogramBuckets ──────────────────────────────── */

enum
{
	OTLP_EHB_FI_OFFSET,
	OTLP_EHB_FI_BUCKET_COUNTS,
	OTLP_EHB_FI_NFIELDS,
};

static const struct otlp_field_spec OTLP_EHB_FIELDS[] = {
	[OTLP_EHB_FI_OFFSET] = { "offset",
		1,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_EHB_FI_BUCKET_COUNTS] = { "bucket_counts",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
};

/* ════════════════════════════════════════════════════════════════
 * Logs signal — opentelemetry-proto logs/v1
 * ════════════════════════════════════════════════════════════════ */

/* ── ExportLogsServiceRequest ─────────────────────────────────── */

enum
{
	OTLP_ELSR_FI_RESOURCE_LOGS,
	OTLP_ELSR_FI_COUNT,
};

static const struct otlp_field_spec OTLP_ELSR_FIELDS[] = {
	[OTLP_ELSR_FI_RESOURCE_LOGS] = { "resource_logs",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
};

/* ── ResourceLogs ─────────────────────────────────────────────── */

enum
{
	OTLP_RL_FI_RESOURCE,
	OTLP_RL_FI_SCOPE_LOGS,
	OTLP_RL_FI_SCHEMA_URL,
	OTLP_RL_FI_COUNT,
};

static const struct otlp_field_spec OTLP_RL_FIELDS[] = {
	[OTLP_RL_FI_RESOURCE] = { "resource",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_RL_FI_SCOPE_LOGS] = { "scope_logs",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_RL_FI_SCHEMA_URL] = { "schema_url",
		3,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── ScopeLogs ────────────────────────────────────────────────── */

enum
{
	OTLP_SL_FI_SCOPE,
	OTLP_SL_FI_LOG_RECORDS,
	OTLP_SL_FI_SCHEMA_URL,
	OTLP_SL_FI_COUNT,
};

static const struct otlp_field_spec OTLP_SL_FIELDS[] = {
	[OTLP_SL_FI_SCOPE] = { "scope",
		1,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_SL_FI_LOG_RECORDS] = { "log_records",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_SL_FI_SCHEMA_URL] = { "schema_url",
		3,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── LogRecord ────────────────────────────────────────────────── */

enum
{
	OTLP_LOG_FI_TIME,
	OTLP_LOG_FI_SEVERITY_NUMBER,
	OTLP_LOG_FI_SEVERITY_TEXT,
	OTLP_LOG_FI_BODY,
	OTLP_LOG_FI_ATTRIBUTES,
	OTLP_LOG_FI_DROPPED_ATTRS,
	OTLP_LOG_FI_FLAGS,
	OTLP_LOG_FI_TRACE_ID,
	OTLP_LOG_FI_SPAN_ID,
	OTLP_LOG_FI_COUNT,
};

static const struct otlp_field_spec OTLP_LOG_FIELDS[] = {
	[OTLP_LOG_FI_TIME] = { "time_unix_nano",
		1,
		OTLP_PB_WIRE_FIXED64,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_LOG_FI_SEVERITY_NUMBER] = { "severity_number",
		2,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_LOG_FI_SEVERITY_TEXT] = { "severity_text",
		3,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	/* Field 4 is reserved in opentelemetry-proto.
	 * observed_time_unix_nano = 11 — not emitted. */
	[OTLP_LOG_FI_BODY] = { "body",
		5,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_LOG_FI_ATTRIBUTES] = { "attributes",
		6,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		true },
	[OTLP_LOG_FI_DROPPED_ATTRS] = { "dropped_attributes_count",
		7,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_LOG_FI_FLAGS] = { "flags",
		8,
		OTLP_PB_WIRE_FIXED32,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_LOG_FI_TRACE_ID] = { "trace_id",
		9,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_LOG_FI_SPAN_ID] = { "span_id",
		10,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

/* ── Export*ServiceResponse + PartialSuccess (decode-only) ────── */

/* Upstream opentelemetry-proto defines one Response/PartialSuccess
 * pair per signal (ExportTrace/ExportMetrics/ExportLogs service),
 * but the three pairs are field-number- and wire-type-identical, so
 * one shared table each (DRY). The rejected field's NAME differs per
 * signal upstream (rejected_spans / rejected_data_points /
 * rejected_log_records); the meaning is the same.
 *
 * These are DECODE-only: the library receives them from a collector
 * (200 OK + PartialSuccess = server-side data loss report); it never
 * encodes them. */

enum
{
	OTLP_EXPSR_FI_PARTIAL_SUCCESS,
	OTLP_EXPSR_FI_COUNT,
};

static const struct otlp_field_spec OTLP_EXPSR_FIELDS[] = {
	[OTLP_EXPSR_FI_PARTIAL_SUCCESS] = { "partial_success",
		5,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

enum
{
	OTLP_EPS_FI_REJECTED,
	OTLP_EPS_FI_ERROR_MESSAGE,
	OTLP_EPS_FI_COUNT,
};

static const struct otlp_field_spec OTLP_EPS_FIELDS[] = {
	[OTLP_EPS_FI_REJECTED] = { "rejected",
		1,
		OTLP_PB_WIRE_VARINT,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
	[OTLP_EPS_FI_ERROR_MESSAGE] = { "error_message",
		2,
		OTLP_PB_WIRE_LEN,
		OTLP_PRESENCE_DEFAULT_OMITTED,
		false },
};

#endif
