/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * OTLP Logs — public API for structured log records with trace correlation.
 *
 * LogRecord is a single message with severity, body, attributes,
 * and optional trace_id/span_id for correlating logs with traces.
 *
 * Lifetime: caller-owned. Construct via otlp_log_record_create();
 * free via otlp_log_record_free().
 *
 * Return codes (every otlp_status_t setter): OTLP_OK on success;
 * OTLP_ERR_NULL for a NULL record or (where applicable) NULL
 * key; OTLP_ERR_NOMEM on allocation failure; OTLP_ERR_OVERFLOW
 * past the 128-distinct-key attribute cap; OTLP_ERR_INVALID_ARGUMENT
 * for all-zero trace/span IDs (W3C) or a NULL entry key in a
 * composite value. Inputs are deep-copied where owned.
 *
 * Thread-safety: single-threaded (same model as spans and metrics).
 */
#ifndef OTLP_C_LOG_H
#define OTLP_C_LOG_H

#include <otlp-c/status.h>
#include <otlp-c/value.h>
#include <otlp-c/visibility.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct otlp_log_record otlp_log_record_t;

	/* OTLP severity numbers (opentelemetry-proto). */
	typedef enum
	{
		OTLP_SEVERITY_UNSPECIFIED = 0,
		OTLP_SEVERITY_TRACE = 1,
		OTLP_SEVERITY_TRACE2 = 2,
		OTLP_SEVERITY_TRACE3 = 3,
		OTLP_SEVERITY_TRACE4 = 4,
		OTLP_SEVERITY_DEBUG = 5,
		OTLP_SEVERITY_DEBUG2 = 6,
		OTLP_SEVERITY_DEBUG3 = 7,
		OTLP_SEVERITY_DEBUG4 = 8,
		OTLP_SEVERITY_INFO = 9,
		OTLP_SEVERITY_INFO2 = 10,
		OTLP_SEVERITY_INFO3 = 11,
		OTLP_SEVERITY_INFO4 = 12,
		OTLP_SEVERITY_WARN = 13,
		OTLP_SEVERITY_WARN2 = 14,
		OTLP_SEVERITY_WARN3 = 15,
		OTLP_SEVERITY_WARN4 = 16,
		OTLP_SEVERITY_ERROR = 17,
		OTLP_SEVERITY_ERROR2 = 18,
		OTLP_SEVERITY_ERROR3 = 19,
		OTLP_SEVERITY_ERROR4 = 20,
		OTLP_SEVERITY_FATAL = 21,
		OTLP_SEVERITY_FATAL2 = 22,
		OTLP_SEVERITY_FATAL3 = 23,
		OTLP_SEVERITY_FATAL4 = 24,
	} otlp_severity_t;

	/* Construct a log record with severity + body string.
	 * `body` may be NULL (empty body). */
	OTLP_C_EXPORT
	otlp_log_record_t *otlp_log_record_create(otlp_severity_t severity,
		const char *body);

	OTLP_C_EXPORT
	void otlp_log_record_free(otlp_log_record_t *lr);

	/* Timestamps. */
	OTLP_C_EXPORT
	otlp_status_t otlp_log_record_set_timestamp(otlp_log_record_t *lr,
		uint64_t unix_nano);
	OTLP_C_EXPORT
	otlp_status_t otlp_log_record_mark_timestamp(otlp_log_record_t *lr);

	/* Trace correlation. Pass pointers to 16-byte / 8-byte arrays.
	 *
	 * Each setter sets only its own flag — callers can correlate to
	 * a trace_id without a span_id (or vice versa) if needed. The
	 * encoder emits each independently on the wire.
	 *
	 * W3C Trace Context §3.1.1/§3.1.2 forbids all-zero IDs; the
	 * library returns OTLP_ERR_INVALID_ARGUMENT for all-zero input
	 * so invalid IDs never reach the wire. */
	OTLP_C_EXPORT
	otlp_status_t otlp_log_record_set_trace_id(otlp_log_record_t *lr,
		const uint8_t *trace_id);
	OTLP_C_EXPORT
	otlp_status_t otlp_log_record_set_span_id(otlp_log_record_t *lr,
		const uint8_t *span_id);

	/* Severity text (e.g. "ERROR"). May be NULL. */
	OTLP_C_EXPORT
	otlp_status_t otlp_log_record_set_severity_text(otlp_log_record_t *lr,
		const char *text);

	/* Attributes (same model as span/metric: a map — last write
	 * wins, type may change; max 128 distinct keys).
	 * String keys and string values must be valid UTF-8 (the
	 * proto3 string contract); invalid input returns
	 * OTLP_ERR_UTF8. bytes values are exempt. */
	OTLP_C_EXPORT
	otlp_status_t otlp_log_record_set_attribute_string(
		otlp_log_record_t *lr,
		const char *key,
		const char *value);
	OTLP_C_EXPORT
	otlp_status_t otlp_log_record_set_attribute_int(otlp_log_record_t *lr,
		const char *key,
		int64_t value);
	OTLP_C_EXPORT
	otlp_status_t otlp_log_record_set_attribute_double(
		otlp_log_record_t *lr,
		const char *key,
		double value);
	OTLP_C_EXPORT
	otlp_status_t otlp_log_record_set_attribute_bool(otlp_log_record_t *lr,
		const char *key,
		bool value);
	OTLP_C_EXPORT
	otlp_status_t otlp_log_record_set_attribute_bytes(otlp_log_record_t *lr,
		const char *key,
		const uint8_t *bytes,
		size_t len);

	/* Set an ArrayValue / KeyValueList attribute (deep-copied;
	 * same upsert semantics as the scalar setters). */
	OTLP_C_EXPORT
	otlp_status_t otlp_log_record_set_attribute_array(otlp_log_record_t *lr,
		const char *key,
		const otlp_value_t *items,
		size_t n);
	OTLP_C_EXPORT
	otlp_status_t otlp_log_record_set_attribute_kvlist(
		otlp_log_record_t *lr,
		const char *key,
		const otlp_kv_t *entries,
		size_t n);

#ifdef __cplusplus
}
#endif

#endif
