/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Exporter-internal: build HTTP POSTs from batches of items.
 * See src/exporter_otel.h.
 *
 * Three builders, one per signal. Each follows the same pattern:
 *   1. Pre-size the encode buffer based on batch count.
 *   2. Encode the corresponding Export*ServiceRequest.
 *   3. Start the HTTP request, reusing a keep-alive socket if
 *      provided, otherwise opening a fresh connection.
 *
 * The pre-sizing and the encode-failure cleanup live here, not in
 * the exporter, so all three signal paths share identical cleanup
 * semantics. The exporter retains ownership of the items between
 * build and outcome — the encoded body is owned by the request, not
 * the items themselves.
 */
#include "exporter_otel.h"
#include "http_client.h"
#include "otlp_messages.h"
#include "otlp_schema.h"
#include "protobuf_decode.h"
#include "protobuf_encode.h"

#include <otlp-c/exporter.h>
#include <otlp-c/log.h>
#include <otlp-c/metric.h>
#include <otlp-c/span.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Common start path: pick _with_socket vs _start based on whether
 * the caller supplied a keep-alive socket. Both variants share the
 * same post-encode flow, so factor it once. */
static otlp_status_t
start_post_common(otlp_http_request_t **out,
	const struct otlp_http_url *url,
	const char *user_agent,
	const uint8_t *body,
	size_t body_len,
	uint32_t connect_timeout_ms,
	uint32_t read_timeout_ms,
	otlp_socket_t *reuse_socket)
{
	if (reuse_socket)
		return otlp_http_request_start_with_socket(out,
			url,
			user_agent,
			body,
			body_len,
			connect_timeout_ms,
			read_timeout_ms,
			reuse_socket);
	return otlp_http_request_start(out,
		url,
		user_agent,
		body,
		body_len,
		connect_timeout_ms,
		read_timeout_ms);
}

otlp_status_t
otlp_exporter_otel_build_span_request(const struct otlp_http_url *url,
	const char *user_agent,
	const char *service_name,
	const struct otlp_attribute *resource_attributes,
	size_t n_resource_attributes,
	const otlp_span_t *const *spans,
	size_t n_spans,
	uint32_t connect_timeout_ms,
	uint32_t read_timeout_ms,
	otlp_socket_t *reuse_socket,
	otlp_http_request_t **out)
{
	struct otlp_pb_buf body = { 0 };
	otlp_status_t st;

	if (!url || !out)
		return OTLP_ERR_NULL;

	st = otlp_pb_buf_init(&body, n_spans * 256 + 1024);
	if (st != OTLP_OK)
		return st;
	st = otlp_encode_export_trace_service_request(&body,
		service_name,
		resource_attributes,
		n_resource_attributes,
		NULL,
		NULL,
		spans,
		n_spans);
	if (st != OTLP_OK)
	{
		otlp_pb_buf_free(&body);
		return st;
	}

	st = start_post_common(out,
		url,
		user_agent,
		body.data,
		body.len,
		connect_timeout_ms,
		read_timeout_ms,
		reuse_socket);
	otlp_pb_buf_free(&body);
	return st;
}

otlp_status_t
otlp_exporter_otel_build_metric_request(const struct otlp_http_url *url,
	const char *user_agent,
	const char *service_name,
	const struct otlp_attribute *resource_attributes,
	size_t n_resource_attributes,
	const otlp_metric_t *const *metrics,
	size_t n_metrics,
	uint32_t connect_timeout_ms,
	uint32_t read_timeout_ms,
	otlp_socket_t *reuse_socket,
	otlp_http_request_t **out)
{
	struct otlp_pb_buf body = { 0 };
	struct otlp_http_url u;
	otlp_status_t st;

	if (!url || !out)
		return OTLP_ERR_NULL;

	st = otlp_pb_buf_init(&body, n_metrics * 128 + 512);
	if (st != OTLP_OK)
		return st;
	st = otlp_encode_export_metrics_service_request(&body,
		service_name,
		resource_attributes,
		n_resource_attributes,
		NULL,
		NULL,
		metrics,
		n_metrics);
	if (st != OTLP_OK)
	{
		otlp_pb_buf_free(&body);
		return st;
	}

	u = *url;
	(void) snprintf(u.path, sizeof(u.path), "/v1/metrics");
	st = start_post_common(out,
		&u,
		user_agent,
		body.data,
		body.len,
		connect_timeout_ms,
		read_timeout_ms,
		reuse_socket);
	otlp_pb_buf_free(&body);
	return st;
}

otlp_status_t
otlp_exporter_otel_build_log_request(const struct otlp_http_url *url,
	const char *user_agent,
	const char *service_name,
	const struct otlp_attribute *resource_attributes,
	size_t n_resource_attributes,
	const otlp_log_record_t *const *logs,
	size_t n_logs,
	uint32_t connect_timeout_ms,
	uint32_t read_timeout_ms,
	otlp_socket_t *reuse_socket,
	otlp_http_request_t **out)
{
	struct otlp_pb_buf body = { 0 };
	struct otlp_http_url u;
	otlp_status_t st;

	if (!url || !out)
		return OTLP_ERR_NULL;

	st = otlp_pb_buf_init(&body, n_logs * 128 + 512);
	if (st != OTLP_OK)
		return st;
	st = otlp_encode_export_logs_service_request(&body,
		service_name,
		resource_attributes,
		n_resource_attributes,
		NULL,
		NULL,
		logs,
		n_logs);
	if (st != OTLP_OK)
	{
		otlp_pb_buf_free(&body);
		return st;
	}

	u = *url;
	(void) snprintf(u.path, sizeof(u.path), "/v1/logs");
	st = start_post_common(out,
		&u,
		user_agent,
		body.data,
		body.len,
		connect_timeout_ms,
		read_timeout_ms,
		reuse_socket);
	otlp_pb_buf_free(&body);
	return st;
}

/* ── Response decode: PartialSuccess ──────────────────────────── */

/* Walk one PartialSuccess submessage. Returns false on malformed
 * input; outputs keep their defaults for absent fields. */
static bool
decode_partial_success_sub(const uint8_t *sub,
	size_t sub_len,
	int64_t *rejected,
	const char **error_message,
	size_t *error_message_len)
{
	const uint32_t rejected_field =
		OTLP_EPS_FIELDS[OTLP_EPS_FI_REJECTED].number;
	const uint32_t message_field =
		OTLP_EPS_FIELDS[OTLP_EPS_FI_ERROR_MESSAGE].number;
	struct otlp_pb_reader r;

	otlp_pb_reader_init(&r, sub, sub_len);
	for (;;)
	{
		uint32_t field;
		int wt;

		if (!otlp_pb_read_key(&r, &field, &wt))
			return r.pos >= r.len; /* clean EOF vs malformed */
		if (field == rejected_field && wt == OTLP_PB_WIRE_VARINT)
		{
			uint64_t v;

			if (!otlp_pb_read_varint(&r, &v))
				return false;
			*rejected = (int64_t) v;
		}
		else if (field == message_field && wt == OTLP_PB_WIRE_LEN)
		{
			const uint8_t *data;
			size_t data_len;

			if (!otlp_pb_read_len(&r, &data, &data_len))
				return false;
			*error_message = (const char *) data;
			*error_message_len = data_len;
		}
		else if (!otlp_pb_skip(&r, wt))
			return false;
	}
}

bool
otlp_exporter_otel_decode_partial_success(const uint8_t *body,
	size_t len,
	int64_t *rejected,
	const char **error_message,
	size_t *error_message_len)
{
	const uint32_t ps_field =
		OTLP_EXPSR_FIELDS[OTLP_EXPSR_FI_PARTIAL_SUCCESS].number;
	struct otlp_pb_reader r;
	bool found = false;

	if (!body && len > 0)
		return false;
	*rejected = 0;
	*error_message = NULL;
	*error_message_len = 0;

	otlp_pb_reader_init(&r, body, len);
	for (;;)
	{
		uint32_t field;
		int wt;

		if (!otlp_pb_read_key(&r, &field, &wt))
			return r.pos >= r.len && found;
		if (field == ps_field && wt == OTLP_PB_WIRE_LEN)
		{
			const uint8_t *sub;
			size_t sub_len;

			if (!otlp_pb_read_len(&r, &sub, &sub_len))
				return false;
			if (!decode_partial_success_sub(sub,
				    sub_len,
				    rejected,
				    error_message,
				    error_message_len))
				return false;
			found = true;
		}
		else if (!otlp_pb_skip(&r, wt))
			return false;
	}
}
