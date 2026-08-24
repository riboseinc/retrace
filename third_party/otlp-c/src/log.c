/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * OTLP LogRecord lifecycle. See include/otlp-c/log.h.
 */
#include <otlp-c/log.h>

#include "internal_util.h"
#include "log_internal.h"
#include "platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define OTLP_LOG_MAX_ATTRS 128

otlp_log_record_t *
otlp_log_record_create(otlp_severity_t severity, const char *body)
{
	struct otlp_log_record *lr = otlp_calloc(1, sizeof(*lr));

	if (!lr)
		return NULL;
	lr->severity = severity;
	if (body && !otlp_str_is_utf8(body))
	{
		otlp_free(lr);
		return NULL;
	}
	lr->body = otlp_dup_str(body ? body : "");
	if (!lr->body)
	{
		otlp_free(lr);
		return NULL;
	}
	return lr;
}

void
otlp_log_record_free(otlp_log_record_t *lr)
{
	if (!lr)
		return;
	otlp_free(lr->severity_text);
	otlp_free(lr->body);
	otlp_attr_vec_free(&lr->attrs);
	otlp_free(lr);
}

otlp_status_t
otlp_log_record_set_timestamp(otlp_log_record_t *lr, uint64_t unix_nano)
{
	if (!lr)
		return OTLP_ERR_NULL;
	lr->timestamp = unix_nano;
	lr->has_timestamp = true;
	return OTLP_OK;
}

otlp_status_t
otlp_log_record_mark_timestamp(otlp_log_record_t *lr)
{
	if (!lr)
		return OTLP_ERR_NULL;
	{
		uint64_t now;

		if (otlp_platform_now_unix_nano(&now) != OTLP_OK)
			return OTLP_ERR_NETWORK;
		lr->timestamp = now;
		lr->has_timestamp = true;
		return OTLP_OK;
	}
}

otlp_status_t
otlp_log_record_set_trace_id(otlp_log_record_t *lr, const uint8_t *trace_id)
{
	if (!lr || !trace_id)
		return OTLP_ERR_NULL;
	/* W3C Trace Context §3.1.1: trace-id MUST NOT be all-zero. */
	if (otlp_id_is_all_zero(trace_id, OTLP_TRACE_ID_LEN))
		return OTLP_ERR_INVALID_ARGUMENT;
	memcpy(lr->trace_id, trace_id, OTLP_TRACE_ID_LEN);
	lr->has_trace_id = true;
	return OTLP_OK;
}

otlp_status_t
otlp_log_record_set_span_id(otlp_log_record_t *lr, const uint8_t *span_id)
{
	if (!lr || !span_id)
		return OTLP_ERR_NULL;
	/* W3C Trace Context §3.1.2: parent-id (span-id) MUST NOT be
	 * all-zero. */
	if (otlp_id_is_all_zero(span_id, OTLP_SPAN_ID_LEN))
		return OTLP_ERR_INVALID_ARGUMENT;
	memcpy(lr->span_id, span_id, OTLP_SPAN_ID_LEN);
	lr->has_span_id = true;
	return OTLP_OK;
}

otlp_status_t
otlp_log_record_set_severity_text(otlp_log_record_t *lr, const char *text)
{
	if (!lr)
		return OTLP_ERR_NULL;
	if (text && !otlp_str_is_utf8(text))
		return OTLP_ERR_UTF8;
	otlp_free(lr->severity_text);
	lr->severity_text = otlp_dup_str(text ? text : "");
	if (text && !lr->severity_text)
		return OTLP_ERR_NOMEM;
	return OTLP_OK;
}

otlp_status_t
otlp_log_record_set_attribute_string(otlp_log_record_t *lr,
	const char *key,
	const char *val)
{
	otlp_value_t v = { .type = OTLP_VALUE_STRING,
		.v = { .string_val = val ? val : "" } };
	if (!lr || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&lr->attrs, OTLP_LOG_MAX_ATTRS, key, &v);
}


otlp_status_t
otlp_log_record_set_attribute_int(otlp_log_record_t *lr,
	const char *key,
	int64_t val)
{
	otlp_value_t v = { .type = OTLP_VALUE_INT64,
		.v = { .int64_val = val } };
	if (!lr || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&lr->attrs, OTLP_LOG_MAX_ATTRS, key, &v);
}


otlp_status_t
otlp_log_record_set_attribute_double(otlp_log_record_t *lr,
	const char *key,
	double val)
{
	otlp_value_t v = { .type = OTLP_VALUE_DOUBLE,
		.v = { .double_val = val } };
	if (!lr || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&lr->attrs, OTLP_LOG_MAX_ATTRS, key, &v);
}


otlp_status_t
otlp_log_record_set_attribute_bool(otlp_log_record_t *lr,
	const char *key,
	bool val)
{
	otlp_value_t v = { .type = OTLP_VALUE_BOOL, .v = { .bool_val = val } };
	if (!lr || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&lr->attrs, OTLP_LOG_MAX_ATTRS, key, &v);
}


otlp_status_t
otlp_log_record_set_attribute_bytes(otlp_log_record_t *lr,
	const char *key,
	const uint8_t *bytes,
	size_t len)
{
	otlp_value_t v = { .type = OTLP_VALUE_BYTES,
		.v = { .bytes_val = { .data = bytes, .len = len } } };
	if (!lr || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&lr->attrs, OTLP_LOG_MAX_ATTRS, key, &v);
}


otlp_status_t
otlp_log_record_set_attribute_array(otlp_log_record_t *lr,
	const char *key,
	const otlp_value_t *items,
	size_t n)
{
	if (!lr || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set_array(
		&lr->attrs, OTLP_LOG_MAX_ATTRS, key, items, n);
}


otlp_status_t
otlp_log_record_set_attribute_kvlist(otlp_log_record_t *lr,
	const char *key,
	const otlp_kv_t *entries,
	size_t n)
{
	if (!lr || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set_kvlist(
		&lr->attrs, OTLP_LOG_MAX_ATTRS, key, entries, n);
}


/* ── Internal accessors ───────────────────────────────────────── */

otlp_severity_t
otlp_log_get_severity(const otlp_log_record_t *lr)
{
	return lr ? lr->severity : OTLP_SEVERITY_UNSPECIFIED;
}
const char *
otlp_log_get_severity_text(const otlp_log_record_t *lr)
{
	return lr ? lr->severity_text : NULL;
}
const char *
otlp_log_get_body(const otlp_log_record_t *lr)
{
	return lr ? lr->body : NULL;
}
uint64_t
otlp_log_get_timestamp(const otlp_log_record_t *lr)
{
	return lr ? lr->timestamp : 0;
}
bool
otlp_log_has_timestamp(const otlp_log_record_t *lr)
{
	return lr ? lr->has_timestamp : false;
}
const uint8_t *
otlp_log_get_trace_id(const otlp_log_record_t *lr)
{
	return lr ? lr->trace_id : NULL;
}
const uint8_t *
otlp_log_get_span_id(const otlp_log_record_t *lr)
{
	return lr ? lr->span_id : NULL;
}
bool
otlp_log_has_trace(const otlp_log_record_t *lr)
{
	return lr ? (lr->has_trace_id || lr->has_span_id) : false;
}
bool
otlp_log_has_trace_id(const otlp_log_record_t *lr)
{
	return lr ? lr->has_trace_id : false;
}
bool
otlp_log_has_span_id(const otlp_log_record_t *lr)
{
	return lr ? lr->has_span_id : false;
}

const struct otlp_attribute *
otlp_log_get_attrs(const otlp_log_record_t *lr, size_t *n)
{
	if (n)
		*n = lr ? lr->attrs.n : 0;
	return lr ? lr->attrs.items : NULL;
}

size_t
otlp_log_struct_size(void)
{
	return sizeof(struct otlp_log_record);
}

otlp_log_record_t *
otlp_log_record_clone(const otlp_log_record_t *src)
{
	struct otlp_log_record *dst;

	if (!src)
		return NULL;
	dst = otlp_calloc(1, sizeof(*dst));
	if (!dst)
		return NULL;
	dst->severity = src->severity;
	dst->severity_text = otlp_dup_str(src->severity_text);
	dst->body = otlp_dup_str(src->body);
	if ((src->severity_text && !dst->severity_text) ||
		(src->body && !dst->body))
		goto fail;
	dst->timestamp = src->timestamp;
	dst->has_timestamp = src->has_timestamp;
	memcpy(dst->trace_id, src->trace_id, OTLP_TRACE_ID_LEN);
	memcpy(dst->span_id, src->span_id, OTLP_SPAN_ID_LEN);
	dst->has_trace_id = src->has_trace_id;
	dst->has_span_id = src->has_span_id;

	if (otlp_attr_vec_copy(&dst->attrs, &src->attrs) != OTLP_OK)
		goto fail;

	return (otlp_log_record_t *) dst;

fail:
	otlp_log_record_free((otlp_log_record_t *) dst);
	return NULL;
}
