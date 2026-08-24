/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * OTLP message encoders. See src/otlp_messages.h for the design.
 *
 * Field numbers and wire types come from src/otlp_schema.h, which
 * is the model-driven single source of truth for the OTLP schema.
 * The encoders below are hand-rolled for clarity but reference
 * schema.h constants (O(N) lookups, but the compiler folds them
 * at -O2). Adding a new field is a one-line schema entry plus an
 * emit call.
 */
#include "otlp_messages.h"
#include "otlp_schema.h"
#include "protobuf_encode.h"
#include "span_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Field-number accessors — single source of truth is otlp_schema.h.
 * Named indices make this robust against table reordering. */
#define ETSR_F_RESOURCE_SPANS \
	OTLP_ETSR_FIELDS[OTLP_ETSR_FI_RESOURCE_SPANS].number
#define RS_F_RESOURCE OTLP_RS_FIELDS[OTLP_RS_FI_RESOURCE].number
#define RS_F_SCOPE_SPANS OTLP_RS_FIELDS[OTLP_RS_FI_SCOPE_SPANS].number
#define R_F_ATTRIBUTES OTLP_R_FIELDS[OTLP_R_FI_ATTRIBUTES].number
#define SS_F_SCOPE OTLP_SS_FIELDS[OTLP_SS_FI_SCOPE].number
#define SS_F_SPANS OTLP_SS_FIELDS[OTLP_SS_FI_SPANS].number
#define IS_F_NAME OTLP_IS_FIELDS[OTLP_IS_FI_NAME].number
#define IS_F_VERSION OTLP_IS_FIELDS[OTLP_IS_FI_VERSION].number
#define SPAN_F_TRACE_ID OTLP_SPAN_FIELDS[OTLP_SPAN_FI_TRACE_ID].number
#define SPAN_F_SPAN_ID OTLP_SPAN_FIELDS[OTLP_SPAN_FI_SPAN_ID].number
#define SPAN_F_TRACE_STATE OTLP_SPAN_FIELDS[OTLP_SPAN_FI_TRACE_STATE].number
#define SPAN_F_PARENT_SPAN_ID \
	OTLP_SPAN_FIELDS[OTLP_SPAN_FI_PARENT_SPAN_ID].number
#define SPAN_F_NAME OTLP_SPAN_FIELDS[OTLP_SPAN_FI_NAME].number
#define SPAN_F_KIND OTLP_SPAN_FIELDS[OTLP_SPAN_FI_KIND].number
#define SPAN_F_START_TIME OTLP_SPAN_FIELDS[OTLP_SPAN_FI_START_TIME].number
#define SPAN_F_END_TIME OTLP_SPAN_FIELDS[OTLP_SPAN_FI_END_TIME].number
#define SPAN_F_ATTRIBUTES OTLP_SPAN_FIELDS[OTLP_SPAN_FI_ATTRIBUTES].number
#define SPAN_F_EVENTS OTLP_SPAN_FIELDS[OTLP_SPAN_FI_EVENTS].number
#define SPAN_F_LINKS OTLP_SPAN_FIELDS[OTLP_SPAN_FI_LINKS].number
#define SPAN_F_STATUS OTLP_SPAN_FIELDS[OTLP_SPAN_FI_STATUS].number
#define STATUS_F_CODE OTLP_STATUS_FIELDS[OTLP_STATUS_FI_CODE].number
#define STATUS_F_MESSAGE OTLP_STATUS_FIELDS[OTLP_STATUS_FI_MESSAGE].number
#define KV_F_KEY OTLP_KV_FIELDS[OTLP_KV_FI_KEY].number
#define KV_F_VALUE OTLP_KV_FIELDS[OTLP_KV_FI_VALUE].number
#define EVENT_F_NAME OTLP_EVENT_FIELDS[OTLP_EVENT_FI_NAME].number
#define EVENT_F_TIME OTLP_EVENT_FIELDS[OTLP_EVENT_FI_TIME].number
#define EVENT_F_ATTRIBUTES OTLP_EVENT_FIELDS[OTLP_EVENT_FI_ATTRIBUTES].number
#define LINK_F_TRACE_ID OTLP_LINK_FIELDS[OTLP_LINK_FI_TRACE_ID].number
#define LINK_F_SPAN_ID OTLP_LINK_FIELDS[OTLP_LINK_FI_SPAN_ID].number
#define LINK_F_ATTRIBUTES OTLP_LINK_FIELDS[OTLP_LINK_FI_ATTRIBUTES].number
#define AV_F_STRING OTLP_AV_FIELDS[OTLP_AV_FI_STRING].number
#define AV_F_BOOL OTLP_AV_FIELDS[OTLP_AV_FI_BOOL].number
#define AV_F_INT64 OTLP_AV_FIELDS[OTLP_AV_FI_INT64].number
#define AV_F_DOUBLE OTLP_AV_FIELDS[OTLP_AV_FI_DOUBLE].number
#define AV_F_BYTES OTLP_AV_FIELDS[OTLP_AV_FI_BYTES].number

/* ── AnyValue ───────────────────────────────────────────────────
 *
 * Always emit the oneof tag for the active variant, even when the
 * value is the type's zero (false, 0, 0.0, ""). The oneof indicator
 * is what tells the consumer which variant was chosen.
 *
 * The field number + wire type come from OTLP_AV_FIELDS[a->type]
 * (table lookup — no switch). The value emission is delegated to a
 * per-type encoder function via attr_encoders[]. Adding a new
 * attribute type (e.g. ArrayValue) is one enum value + one table
 * entry + one encode function — no switch to modify. OCP.
 */

typedef otlp_status_t (*otlp_attr_encode_fn)(struct otlp_pb_buf *,
	const struct otlp_attribute *);

static otlp_status_t
encode_attr_string(struct otlp_pb_buf *buf, const struct otlp_attribute *a)
{
	const char *s = a->v.string_val ? a->v.string_val : "";

	return otlp_pb_string(buf, s);
}

static otlp_status_t
encode_attr_bool(struct otlp_pb_buf *buf, const struct otlp_attribute *a)
{
	return otlp_pb_varint(buf, a->v.bool_val ? 1ULL : 0ULL);
}

static otlp_status_t
encode_attr_int64(struct otlp_pb_buf *buf, const struct otlp_attribute *a)
{
	return otlp_pb_varint(buf, (uint64_t) a->v.int64_val);
}

static otlp_status_t
encode_attr_double(struct otlp_pb_buf *buf, const struct otlp_attribute *a)
{
	uint64_t bits;

	memcpy(&bits, &a->v.double_val, sizeof(bits));
	return otlp_pb_fixed64(buf, bits);
}

static otlp_status_t
encode_attr_bytes(struct otlp_pb_buf *buf, const struct otlp_attribute *a)
{
	const uint8_t *p = a->v.bytes_val.data;
	size_t len = a->v.bytes_val.len;

	return otlp_pb_bytes(buf, p ? p : (const uint8_t *) "", len);
}

/* Forward decls: array + kvlist encoders recurse into otlp_encode_any_value. */
static otlp_status_t
encode_attr_array(struct otlp_pb_buf *buf, const struct otlp_attribute *a)
{
	/* ArrayValue { repeated AnyValue values = 1; }
	 * otlp_encode_any_value already wrote the tag; a LEN field
	 * needs its length varint, then the body — build the body
	 * first so the length is exact (missing the length varint
	 * produced a malformed frame; caught by prop_attr_array_wire,
	 * v0.5.74). */
	const struct otlp_attr_array *arr = a->v.array_val;
	struct otlp_pb_buf body = { 0 };
	otlp_status_t st;
	size_t i;
	const uint32_t values_field =
		OTLP_AV_ARRAY_FIELDS[OTLP_AV_ARRAY_FI_VALUES].number;

	if (!arr || arr->n == 0)
		return otlp_pb_bytes(buf, (const uint8_t *) "", 0);
	st = otlp_pb_buf_init(&body, 0);
	if (st != OTLP_OK)
		return st;
	for (i = 0; i < arr->n; i++)
	{
		struct otlp_pb_buf item = { 0 };

		st = otlp_pb_buf_init(&item, 0);
		if (st == OTLP_OK)
			st = otlp_encode_any_value(&item, &arr->items[i]);
		if (st == OTLP_OK)
			st = otlp_pb_field_message(
				&body, values_field, item.data, item.len);
		otlp_pb_buf_free(&item);
		if (st != OTLP_OK)
			goto out;
	}
	st = otlp_pb_bytes(buf, body.data, body.len);
out:
	otlp_pb_buf_free(&body);
	return st;
}

static otlp_status_t
encode_attr_kvlist(struct otlp_pb_buf *buf, const struct otlp_attribute *a)
{
	/* KeyValueList { repeated KeyValue values = 1; }
	 * See encode_attr_array: the tag is already written; this
	 * writes the length varint + body. */
	const struct otlp_attr_kvlist *kvl = a->v.kvlist_val;
	struct otlp_pb_buf body = { 0 };
	otlp_status_t st;
	size_t i;
	const uint32_t values_field =
		OTLP_KVLIST_FIELDS[OTLP_KVLIST_FI_VALUES].number;

	if (!kvl || kvl->n == 0)
		return otlp_pb_bytes(buf, (const uint8_t *) "", 0);
	st = otlp_pb_buf_init(&body, 0);
	if (st != OTLP_OK)
		return st;
	for (i = 0; i < kvl->n; i++)
	{
		struct otlp_pb_buf entry = { 0 };

		st = otlp_pb_buf_init(&entry, 0);
		if (st == OTLP_OK)
			st = otlp_encode_key_value(&entry,
				kvl->entries[i].key,
				&kvl->entries[i].value);
		if (st == OTLP_OK)
			st = otlp_pb_field_message(
				&body, values_field, entry.data, entry.len);
		otlp_pb_buf_free(&entry);
		if (st != OTLP_OK)
			goto out;
	}
	st = otlp_pb_bytes(buf, body.data, body.len);
out:
	otlp_pb_buf_free(&body);
	return st;
}

/* Dispatch table: indexed by enum otlp_attr_type, which now matches
 * OTLP_AV_FI_* indices. All seven variants are populated — the
 * AnyValue oneof is fully covered. OCP. */
static const otlp_attr_encode_fn attr_encoders[] = {
	[OTLP_ATTR_STRING] = encode_attr_string,
	[OTLP_ATTR_BOOL] = encode_attr_bool,
	[OTLP_ATTR_INT64] = encode_attr_int64,
	[OTLP_ATTR_DOUBLE] = encode_attr_double,
	[OTLP_ATTR_ARRAY] = encode_attr_array,
	[OTLP_ATTR_KVLIST] = encode_attr_kvlist,
	[OTLP_ATTR_BYTES] = encode_attr_bytes,
};

otlp_status_t
otlp_encode_any_value(struct otlp_pb_buf *buf, const struct otlp_attribute *a)
{
	const struct otlp_field_spec *f;
	otlp_status_t st;

	if ((unsigned) a->type >= OTLP_AV_FI_COUNT || !attr_encoders[a->type])
		return OTLP_ERR_INVALID_ARGUMENT;

	f = &OTLP_AV_FIELDS[a->type];
	st = otlp_pb_tag(buf, f->number, f->wire_type);
	if (st != OTLP_OK)
		return st;
	return attr_encoders[a->type](buf, a);
}

/* ── KeyValue (public, for tests) ─────────────────────────────── */

otlp_status_t
otlp_encode_key_value(struct otlp_pb_buf *out,
	const char *key,
	const struct otlp_attribute *attr)
{
	struct otlp_pb_buf val_buf = { 0 };
	otlp_status_t st;

	if (!out || !attr)
		return OTLP_ERR_NULL;

	st = otlp_pb_buf_init(&val_buf, 0);
	if (st != OTLP_OK)
		return st;
	st = otlp_encode_any_value(&val_buf, attr);
	if (st != OTLP_OK)
		goto out;

	/* key (always emit, may be empty). */
	st = otlp_pb_tag(out, KV_F_KEY, OTLP_PB_WIRE_LEN);
	if (st != OTLP_OK)
		goto out;
	st = otlp_pb_string(out, key ? key : "");
	if (st != OTLP_OK)
		goto out;

	/* value (sub-message; even an empty AnyValue is emitted). */
	st = otlp_pb_field_message(out, KV_F_VALUE, val_buf.data, val_buf.len);

out:
	otlp_pb_buf_free(&val_buf);
	return st;
}

/* ── Status (private) ─────────────────────────────────────────── */

static otlp_status_t
emit_status(struct otlp_pb_buf *parent,
	uint32_t field_num,
	otlp_status_code_t code,
	const char *message)
{
	struct otlp_pb_buf sub = { 0 };
	otlp_status_t st;

	/* UNSET (0) is the default; omit the entire Status sub-message. */
	if (code == OTLP_STATUS_CODE_UNSET && !(message && message[0]))
		return OTLP_OK;

	st = otlp_pb_buf_init(&sub, 0);
	if (st != OTLP_OK)
		return st;

	if (code != OTLP_STATUS_CODE_UNSET)
	{
		st = otlp_pb_tag(&sub, STATUS_F_CODE, OTLP_PB_WIRE_VARINT);
		if (st != OTLP_OK)
			goto out;
		st = otlp_pb_varint(&sub, (uint64_t) code);
		if (st != OTLP_OK)
			goto out;
	}
	if (message && message[0])
	{
		st = otlp_pb_tag(&sub, STATUS_F_MESSAGE, OTLP_PB_WIRE_LEN);
		if (st != OTLP_OK)
			goto out;
		st = otlp_pb_string(&sub, message);
		if (st != OTLP_OK)
			goto out;
	}

	st = otlp_pb_field_message(parent, field_num, sub.data, sub.len);

out:
	otlp_pb_buf_free(&sub);
	return st;
}

/* ── Span body (public, for tests) ────────────────────────────── */

otlp_status_t
otlp_encode_span_body(struct otlp_pb_buf *out, const otlp_span_t *span)
{
	const uint8_t *trace_id;
	const uint8_t *span_id;
	const char *name;
	otlp_span_kind_t kind;
	size_t n_attrs;
	const struct otlp_attribute *attrs;
	size_t i;
	otlp_status_t st;

	if (!out || !span)
		return OTLP_ERR_NULL;

	trace_id = otlp_span_get_trace_id(span);
	span_id = otlp_span_get_span_id(span);

	/* trace_id (field 1) — always emit (required by spec). */
	st = otlp_pb_tag(out, SPAN_F_TRACE_ID, OTLP_PB_WIRE_LEN);
	if (st != OTLP_OK)
		return st;
	st = otlp_pb_bytes(out, trace_id, OTLP_TRACE_ID_LEN);
	if (st != OTLP_OK)
		return st;

	/* span_id (field 2) — always emit. */
	st = otlp_pb_tag(out, SPAN_F_SPAN_ID, OTLP_PB_WIRE_LEN);
	if (st != OTLP_OK)
		return st;
	st = otlp_pb_bytes(out, span_id, OTLP_SPAN_ID_LEN);
	if (st != OTLP_OK)
		return st;

	/* parent_span_id (field 4) — only if has_parent. */
	if (otlp_span_has_parent(span))
	{
		st = otlp_pb_tag(out, SPAN_F_PARENT_SPAN_ID, OTLP_PB_WIRE_LEN);
		if (st != OTLP_OK)
			return st;
		st = otlp_pb_bytes(out,
			otlp_span_get_parent_span_id(span),
			OTLP_SPAN_ID_LEN);
		if (st != OTLP_OK)
			return st;
	}

	/* trace_state (field 3) — only if non-empty. */
	{
		const char *ts = otlp_span_get_trace_state(span);

		if (ts && ts[0])
		{
			st = otlp_pb_field_string(out, SPAN_F_TRACE_STATE, ts);
			if (st != OTLP_OK)
				return st;
		}
	}

	/* name (field 5) — skip if empty. */
	name = otlp_span_get_name(span);
	if (name && name[0])
	{
		st = otlp_pb_field_string(out, SPAN_F_NAME, name);
		if (st != OTLP_OK)
			return st;
	}

	/* kind (field 6) — skip if UNSPECIFIED (protobuf default). */
	kind = otlp_span_get_kind(span);
	if (kind != OTLP_SPAN_KIND_UNSPECIFIED)
	{
		st = otlp_pb_field_varint(out, SPAN_F_KIND, (uint64_t) kind);
		if (st != OTLP_OK)
			return st;
	}

	/* start_time (field 7) — emit unconditionally if set. */
	if (otlp_span_has_start_time(span))
	{
		uint64_t t = otlp_span_get_start_time(span);
		st = otlp_pb_tag(out, SPAN_F_START_TIME, OTLP_PB_WIRE_FIXED64);
		if (st != OTLP_OK)
			return st;
		st = otlp_pb_fixed64(out, t);
		if (st != OTLP_OK)
			return st;
	}

	/* end_time (field 8). */
	if (otlp_span_has_end_time(span))
	{
		uint64_t t = otlp_span_get_end_time(span);
		st = otlp_pb_tag(out, SPAN_F_END_TIME, OTLP_PB_WIRE_FIXED64);
		if (st != OTLP_OK)
			return st;
		st = otlp_pb_fixed64(out, t);
		if (st != OTLP_OK)
			return st;
	}

	/* attributes (field 9, repeated). */
	attrs = otlp_span_get_attrs(span, &n_attrs);
	for (i = 0; i < n_attrs; i++)
	{
		struct otlp_pb_buf kv = { 0 };
		st = otlp_pb_buf_init(&kv, 0);
		if (st != OTLP_OK)
			return st;
		st = otlp_encode_key_value(&kv, attrs[i].key, &attrs[i]);
		if (st == OTLP_OK)
			st = otlp_pb_field_message(
				out, SPAN_F_ATTRIBUTES, kv.data, kv.len);
		otlp_pb_buf_free(&kv);
		if (st != OTLP_OK)
			return st;
	}

	/* events (field 11, repeated). Each event emits name + time. */
	{
		size_t n_events;
		const struct otlp_event *events =
			otlp_span_get_events(span, &n_events);

		for (i = 0; i < n_events; i++)
		{
			struct otlp_pb_buf ev = { 0 };
			st = otlp_pb_buf_init(&ev, 0);
			if (st != OTLP_OK)
				return st;
			if (events[i].name && events[i].name[0])
			{
				st = otlp_pb_field_string(
					&ev, EVENT_F_NAME, events[i].name);
				if (st != OTLP_OK)
				{
					otlp_pb_buf_free(&ev);
					return st;
				}
			}
			st = otlp_pb_field_fixed64(
				&ev, EVENT_F_TIME, events[i].time_unix_nano);
			if (st == OTLP_OK)
				st = otlp_emit_attributes(&ev,
					EVENT_F_ATTRIBUTES,
					events[i].attrs.items,
					events[i].attrs.n);
			if (st == OTLP_OK)
				st = otlp_pb_field_message(
					out, SPAN_F_EVENTS, ev.data, ev.len);
			otlp_pb_buf_free(&ev);
			if (st != OTLP_OK)
				return st;
		}
	}

	/* links (field 13, repeated). Each link emits trace_id + span_id. */
	{
		size_t n_links;
		const struct otlp_link *links =
			otlp_span_get_links(span, &n_links);

		for (i = 0; i < n_links; i++)
		{
			struct otlp_pb_buf lk = { 0 };
			st = otlp_pb_buf_init(&lk, 0);
			if (st != OTLP_OK)
				return st;
			st = otlp_pb_field_bytes(&lk,
				LINK_F_TRACE_ID,
				links[i].trace_id,
				OTLP_TRACE_ID_LEN);
			if (st == OTLP_OK)
				st = otlp_pb_field_bytes(&lk,
					LINK_F_SPAN_ID,
					links[i].span_id,
					OTLP_SPAN_ID_LEN);
			if (st == OTLP_OK)
				st = otlp_emit_attributes(&lk,
					LINK_F_ATTRIBUTES,
					links[i].attrs.items,
					links[i].attrs.n);
			if (st == OTLP_OK)
				st = otlp_pb_field_message(
					out, SPAN_F_LINKS, lk.data, lk.len);
			otlp_pb_buf_free(&lk);
			if (st != OTLP_OK)
				return st;
		}
	}

	/* status (field 15) — omitted for UNSET. */
	st = emit_status(out,
		SPAN_F_STATUS,
		otlp_span_get_status_code(span),
		otlp_span_get_status_message(span));
	if (st != OTLP_OK)
		return st;

	/* flags (field 16, fixed32) — W3C trace-flags. Emit when sampled
	 * so the wire value is 0x01 (the protobuf3 default 0x00 means
	 * "not sampled", so omission suffices for unsampled spans). */
	if (otlp_span_is_sampled(span))
	{
		st = otlp_pb_tag(out,
			OTLP_SPAN_FIELDS[OTLP_SPAN_FI_FLAGS].number,
			OTLP_PB_WIRE_FIXED32);
		if (st != OTLP_OK)
			return st;
		st = otlp_pb_fixed32(out, 0x01);
		if (st != OTLP_OK)
			return st;
	}
	return OTLP_OK;
}

/* ── Resource / InstrumentationScope / ScopeSpans / ResourceSpans ─ */

otlp_status_t
otlp_emit_resource(struct otlp_pb_buf *parent,
	uint32_t field_num,
	const char *service_name,
	const struct otlp_attribute *attrs,
	size_t n_attrs)
{
	struct otlp_pb_buf sub = { 0 };
	otlp_status_t st;
	bool have_service = (service_name && service_name[0]);
	bool have_attrs = (attrs && n_attrs > 0);

	if (!have_service && !have_attrs)
		return OTLP_OK;

	st = otlp_pb_buf_init(&sub, 0);
	if (st != OTLP_OK)
		return st;

	if (have_service)
	{
		struct otlp_attribute svc_attr = {
			.key = NULL,
			.type = OTLP_ATTR_STRING,
			.v.string_val = (char *) service_name,
		};
		struct otlp_pb_buf kv = { 0 };
		st = otlp_pb_buf_init(&kv, 0);
		if (st != OTLP_OK)
			goto out;
		st = otlp_encode_key_value(&kv, "service.name", &svc_attr);
		if (st == OTLP_OK)
			st = otlp_pb_field_message(
				&sub, R_F_ATTRIBUTES, kv.data, kv.len);
		otlp_pb_buf_free(&kv);
		if (st != OTLP_OK)
			goto out;
	}

	for (size_t i = 0; i < n_attrs; i++)
	{
		struct otlp_pb_buf kv = { 0 };
		const char *key = attrs[i].key ? attrs[i].key : "";

		if (!key[0])
			continue;

		st = otlp_pb_buf_init(&kv, 0);
		if (st != OTLP_OK)
			goto out;
		st = otlp_encode_key_value(&kv, key, &attrs[i]);
		if (st == OTLP_OK)
			st = otlp_pb_field_message(
				&sub, R_F_ATTRIBUTES, kv.data, kv.len);
		otlp_pb_buf_free(&kv);
		if (st != OTLP_OK)
			goto out;
	}

	st = otlp_pb_field_message(parent, field_num, sub.data, sub.len);

out:
	otlp_pb_buf_free(&sub);
	return st;
}

otlp_status_t
otlp_emit_instrumentation_scope(struct otlp_pb_buf *parent,
	uint32_t field_num,
	const char *name,
	const char *version)
{
	struct otlp_pb_buf sub = { 0 };
	otlp_status_t st;

	if (!(name && name[0]) && !(version && version[0]))
		return OTLP_OK;

	st = otlp_pb_buf_init(&sub, 0);
	if (st != OTLP_OK)
		return st;

	if (name && name[0])
	{
		st = otlp_pb_field_string(&sub, IS_F_NAME, name);
		if (st != OTLP_OK)
			goto out;
	}
	if (version && version[0])
	{
		st = otlp_pb_field_string(&sub, IS_F_VERSION, version);
		if (st != OTLP_OK)
			goto out;
	}

	st = otlp_pb_field_message(parent, field_num, sub.data, sub.len);

out:
	otlp_pb_buf_free(&sub);
	return st;
}

/* ── Attributes (shared, DRY across signals) ───────────────────── */

otlp_status_t
otlp_emit_attributes(struct otlp_pb_buf *parent,
	uint32_t field_num,
	const struct otlp_attribute *attrs,
	size_t n_attrs)
{
	size_t i;

	if (!parent)
		return OTLP_ERR_NULL;
	for (i = 0; i < n_attrs; i++)
	{
		struct otlp_pb_buf kv = { 0 };
		otlp_status_t st;

		st = otlp_pb_buf_init(&kv, 0);
		if (st != OTLP_OK)
			return st;
		st = otlp_encode_key_value(&kv, attrs[i].key, &attrs[i]);
		if (st == OTLP_OK)
			st = otlp_pb_field_message(
				parent, field_num, kv.data, kv.len);
		otlp_pb_buf_free(&kv);
		if (st != OTLP_OK)
			return st;
	}
	return OTLP_OK;
}

static otlp_status_t
emit_scope_spans(struct otlp_pb_buf *parent,
	uint32_t field_num,
	const char *scope_name,
	const char *scope_version,
	const otlp_span_t *const *spans,
	size_t n_spans)
{
	struct otlp_pb_buf sub = { 0 };
	otlp_status_t st;
	size_t i;

	st = otlp_pb_buf_init(&sub, 0);
	if (st != OTLP_OK)
		return st;

	st = otlp_emit_instrumentation_scope(
		&sub, SS_F_SCOPE, scope_name, scope_version);
	if (st != OTLP_OK)
		goto out;

	for (i = 0; i < n_spans; i++)
	{
		struct otlp_pb_buf sp = { 0 };
		st = otlp_pb_buf_init(&sp, 0);
		if (st != OTLP_OK)
			goto out;
		st = otlp_encode_span_body(&sp, spans[i]);
		if (st == OTLP_OK)
			st = otlp_pb_field_message(
				&sub, SS_F_SPANS, sp.data, sp.len);
		otlp_pb_buf_free(&sp);
		if (st != OTLP_OK)
			goto out;
	}

	if (sub.len > 0)
		st = otlp_pb_field_message(
			parent, field_num, sub.data, sub.len);

out:
	otlp_pb_buf_free(&sub);
	return st;
}

static otlp_status_t
emit_resource_spans(struct otlp_pb_buf *parent,
	uint32_t field_num,
	const char *service_name,
	const struct otlp_attribute *resource_attributes,
	size_t n_resource_attributes,
	const char *scope_name,
	const char *scope_version,
	const otlp_span_t *const *spans,
	size_t n_spans)
{
	struct otlp_pb_buf sub = { 0 };
	otlp_status_t st;

	st = otlp_pb_buf_init(&sub, 0);
	if (st != OTLP_OK)
		return st;

	st = otlp_emit_resource(&sub,
		RS_F_RESOURCE,
		service_name,
		resource_attributes,
		n_resource_attributes);
	if (st != OTLP_OK)
		goto out;

	st = emit_scope_spans(&sub,
		RS_F_SCOPE_SPANS,
		scope_name,
		scope_version,
		spans,
		n_spans);
	if (st != OTLP_OK)
		goto out;

	if (sub.len > 0)
		st = otlp_pb_field_message(
			parent, field_num, sub.data, sub.len);

out:
	otlp_pb_buf_free(&sub);
	return st;
}

/* ── Top-level encoder ────────────────────────────────────────── */

otlp_status_t
otlp_encode_export_trace_service_request(struct otlp_pb_buf *out,
	const char *service_name,
	const struct otlp_attribute *resource_attributes,
	size_t n_resource_attributes,
	const char *scope_name,
	const char *scope_version,
	const otlp_span_t *const *spans,
	size_t n_spans)
{
	if (!out)
		return OTLP_ERR_NULL;

	/* Empty request: zero spans, no service name, no attrs → zero bytes. */
	if (n_spans == 0 && !(service_name && service_name[0]) &&
		!(resource_attributes && n_resource_attributes > 0))
		return OTLP_OK;

	return emit_resource_spans(out,
		ETSR_F_RESOURCE_SPANS,
		service_name,
		resource_attributes,
		n_resource_attributes,
		scope_name,
		scope_version,
		spans,
		n_spans);
}
