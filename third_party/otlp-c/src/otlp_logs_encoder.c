/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * OTLP Logs encoder. Produces ExportLogsServiceRequest wire bytes.
 *
 * All field numbers come from src/otlp_schema.h (single source of
 * truth). Reuses shared helpers from otlp_messages.c (otlp_emit_resource,
 * otlp_emit_instrumentation_scope, otlp_encode_any_value,
 * otlp_encode_key_value) so the resource/scope envelope is identical
 * across signals. DRY.
 */
#include "log_internal.h"
#include "otlp_messages.h"
#include "otlp_schema.h"
#include "protobuf_encode.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ELSR_F_RESOURCE_LOGS OTLP_ELSR_FIELDS[OTLP_ELSR_FI_RESOURCE_LOGS].number
#define RL_F_RESOURCE OTLP_RL_FIELDS[OTLP_RL_FI_RESOURCE].number
#define RL_F_SCOPE_LOGS OTLP_RL_FIELDS[OTLP_RL_FI_SCOPE_LOGS].number
#define SL_F_SCOPE OTLP_SL_FIELDS[OTLP_SL_FI_SCOPE].number
#define SL_F_LOG_RECORDS OTLP_SL_FIELDS[OTLP_SL_FI_LOG_RECORDS].number
#define LOG_F_TIME OTLP_LOG_FIELDS[OTLP_LOG_FI_TIME].number
#define LOG_F_SEVERITY_NUMBER \
	OTLP_LOG_FIELDS[OTLP_LOG_FI_SEVERITY_NUMBER].number
#define LOG_F_SEVERITY_TEXT OTLP_LOG_FIELDS[OTLP_LOG_FI_SEVERITY_TEXT].number
#define LOG_F_BODY OTLP_LOG_FIELDS[OTLP_LOG_FI_BODY].number
#define LOG_F_ATTRIBUTES OTLP_LOG_FIELDS[OTLP_LOG_FI_ATTRIBUTES].number
#define LOG_F_TRACE_ID OTLP_LOG_FIELDS[OTLP_LOG_FI_TRACE_ID].number
#define LOG_F_SPAN_ID OTLP_LOG_FIELDS[OTLP_LOG_FI_SPAN_ID].number

static otlp_status_t
emit_log_record(struct otlp_pb_buf *parent,
	uint32_t field_num,
	const otlp_log_record_t *lr)
{
	struct otlp_pb_buf sub = { 0 };
	otlp_status_t st;
	size_t n;
	const struct otlp_attribute *attrs = otlp_log_get_attrs(lr, &n);

	st = otlp_pb_buf_init(&sub, 0);
	if (st != OTLP_OK)
		return st;

	if (otlp_log_has_timestamp(lr))
	{
		st = otlp_pb_field_fixed64(
			&sub, LOG_F_TIME, otlp_log_get_timestamp(lr));
		if (st != OTLP_OK)
			goto out;
	}

	{
		otlp_severity_t sev = otlp_log_get_severity(lr);

		if (sev != OTLP_SEVERITY_UNSPECIFIED)
		{
			st = otlp_pb_field_varint(
				&sub, LOG_F_SEVERITY_NUMBER, (uint64_t) sev);
			if (st != OTLP_OK)
				goto out;
		}
	}

	{
		const char *t = otlp_log_get_severity_text(lr);

		if (t && t[0])
		{
			st = otlp_pb_field_string(&sub, LOG_F_SEVERITY_TEXT, t);
			if (st != OTLP_OK)
				goto out;
		}
	}

	/* body: AnyValue oneof (string variant). */
	{
		const char *body = otlp_log_get_body(lr);
		struct otlp_attribute av = {
			.type = OTLP_ATTR_STRING,
			.v.string_val = (char *) (body ? body : ""),
		};
		struct otlp_pb_buf av_buf = { 0 };

		if (body && body[0])
		{
			st = otlp_pb_buf_init(&av_buf, 0);
			if (st != OTLP_OK)
				goto out;
			st = otlp_encode_any_value(&av_buf, &av);
			if (st == OTLP_OK)
				st = otlp_pb_field_message(&sub,
					LOG_F_BODY,
					av_buf.data,
					av_buf.len);
			otlp_pb_buf_free(&av_buf);
			if (st != OTLP_OK)
				goto out;
		}
	}

	st = otlp_emit_attributes(&sub, LOG_F_ATTRIBUTES, attrs, n);
	if (st != OTLP_OK)
		goto out;

	/* trace_id (field 9) and span_id (field 10). The two flags
	 * are independent so a caller can correlate to a trace_id
	 * without a span_id (or vice versa) — unusual but valid. The
	 * previous design used a single has_trace flag set by either
	 * setter, which silently emitted all-zero bytes for the
	 * unset member; that's a valid proto bytes value but an
	 * invalid W3C trace_id. */
	if (otlp_log_has_trace_id(lr))
	{
		st = otlp_pb_field_bytes(&sub,
			LOG_F_TRACE_ID,
			otlp_log_get_trace_id(lr),
			OTLP_TRACE_ID_LEN);
		if (st != OTLP_OK)
			goto out;
	}
	if (otlp_log_has_span_id(lr))
	{
		st = otlp_pb_field_bytes(&sub,
			LOG_F_SPAN_ID,
			otlp_log_get_span_id(lr),
			OTLP_SPAN_ID_LEN);
		if (st != OTLP_OK)
			goto out;
	}

	st = otlp_pb_field_message(parent, field_num, sub.data, sub.len);

out:
	otlp_pb_buf_free(&sub);
	return st;
}

otlp_status_t
otlp_encode_export_logs_service_request(struct otlp_pb_buf *out,
	const char *service_name,
	const struct otlp_attribute *resource_attributes,
	size_t n_resource_attributes,
	const char *scope_name,
	const char *scope_version,
	const otlp_log_record_t *const *logs,
	size_t n_logs)
{
	struct otlp_pb_buf rl = { 0 }, sl = { 0 };
	otlp_status_t st;
	size_t i;

	if (!out)
		return OTLP_ERR_NULL;
	if (n_logs == 0 && !(service_name && service_name[0]) &&
		!(resource_attributes && n_resource_attributes > 0))
		return OTLP_OK;

	st = otlp_pb_buf_init(&rl, 0);
	if (st != OTLP_OK)
		return st;
	st = otlp_pb_buf_init(&sl, 0);
	if (st != OTLP_OK)
		goto out_rl;

	st = otlp_emit_resource(&rl,
		RL_F_RESOURCE,
		service_name,
		resource_attributes,
		n_resource_attributes);
	if (st != OTLP_OK)
		goto out_sl;

	st = otlp_emit_instrumentation_scope(
		&sl, SL_F_SCOPE, scope_name, scope_version);
	if (st != OTLP_OK)
		goto out_sl;

	for (i = 0; i < n_logs; i++)
	{
		st = emit_log_record(&sl, SL_F_LOG_RECORDS, logs[i]);
		if (st != OTLP_OK)
			goto out_sl;
	}

	if (sl.len > 0)
		st = otlp_pb_field_message(
			&rl, RL_F_SCOPE_LOGS, sl.data, sl.len);
	if (st == OTLP_OK)
		st = otlp_pb_field_message(
			out, ELSR_F_RESOURCE_LOGS, rl.data, rl.len);

out_sl:
	otlp_pb_buf_free(&sl);
out_rl:
	otlp_pb_buf_free(&rl);
	return st;
}
