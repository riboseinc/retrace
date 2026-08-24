/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Span lifecycle and attribute storage.
 *
 * A span owns: its name, its status message, and any heap-allocated
 * attribute payload (strings, byte arrays). The fixed-size fields
 * (IDs, times, kind) are inline. Attributes are stored in a fixed-
 * cap inline array (default 128); overflow returns OTLP_ERR_OVERFLOW.
 *
 * Thread-safety: spans are single-threaded by API contract. The
 * caller builds, mutates, and frees a span on one thread (or
 * synchronizes ownership transfer explicitly). The exporter copies
 * spans into its queue; the original is then free to be freed.
 */
#include <otlp-c/span.h>

#include "internal_util.h"
#include "span_internal.h"
#include "platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef OTLP_SPAN_MAX_ATTRIBUTES
#define OTLP_SPAN_MAX_ATTRIBUTES 128
#endif
#ifndef OTLP_SPAN_MAX_EVENTS
#define OTLP_SPAN_MAX_EVENTS 64
#endif
#ifndef OTLP_SPAN_MAX_LINKS
#define OTLP_SPAN_MAX_LINKS 64
#endif

struct otlp_span
{
	char *name; /* owned */
	uint8_t trace_id[OTLP_TRACE_ID_LEN];
	uint8_t span_id[OTLP_SPAN_ID_LEN];
	uint8_t parent_span_id[OTLP_SPAN_ID_LEN];
	bool has_parent;
	uint64_t start_time_unix_nano;
	uint64_t end_time_unix_nano;
	bool has_start_time;
	bool has_end_time;
	otlp_span_kind_t kind;
	struct otlp_attr_vec attrs;
	otlp_status_code_t status_code;
	char *status_message; /* owned, may be NULL */
	bool sampled;
	char *trace_state; /* owned, may be NULL */
	/* Events/links: grow-on-demand heap arrays (4 -> 8 -> ... slots,
	 * bounded by the caps). A span with no events/links — the
	 * common case — pays two NULL pointers, not 5.6KB of inline
	 * headers (v0.5.76; same model as the attribute vectors). */
	struct otlp_event *events;
	size_t n_events;
	size_t cap_events;
	struct otlp_link *links;
	size_t n_links;
	size_t cap_links;
};

/* ── Internal helpers ─────────────────────────────────────────── */

/* ── Lifecycle ────────────────────────────────────────────────── */

otlp_span_t *
otlp_span_create(const char *name)
{
	otlp_span_t *span;
	char *name_copy;

	span = otlp_malloc(sizeof(*span));
	if (!span)
		return NULL;
	memset(span, 0, sizeof(*span));

	if (name && !otlp_str_is_utf8(name))
	{
		otlp_free(span);
		return NULL;
	}
	name_copy = otlp_dup_str(name ? name : "");
	if (!name_copy)
	{
		otlp_free(span);
		return NULL;
	}
	span->name = name_copy;
	span->kind = OTLP_SPAN_KIND_INTERNAL;
	span->status_code = OTLP_STATUS_CODE_UNSET;
	span->sampled = true;
	return span;
}

void
otlp_span_free(otlp_span_t *span)
{
	size_t i;

	if (!span)
		return;
	otlp_free(span->name);
	otlp_free(span->status_message);
	otlp_free(span->trace_state);
	for (i = 0; i < span->n_events; i++)
	{
		otlp_free(span->events[i].name);
		otlp_attr_vec_free(&span->events[i].attrs);
	}
	otlp_free(span->events);
	for (i = 0; i < span->n_links; i++)
		otlp_attr_vec_free(&span->links[i].attrs);
	otlp_free(span->links);
	otlp_attr_vec_free(&span->attrs);
	otlp_free(span);
}

/* ── Identity ─────────────────────────────────────────────────── */

otlp_status_t
otlp_span_set_trace_id(otlp_span_t *span, const uint8_t *trace_id)
{
	if (!span)
		return OTLP_ERR_NULL;
	if (!trace_id)
		return OTLP_ERR_NULL;
	/* W3C Trace Context §3.1.1: trace-id MUST NOT be all-zero. */
	if (otlp_id_is_all_zero(trace_id, OTLP_TRACE_ID_LEN))
		return OTLP_ERR_INVALID_ARGUMENT;
	memcpy(span->trace_id, trace_id, OTLP_TRACE_ID_LEN);
	return OTLP_OK;
}

otlp_status_t
otlp_span_set_span_id(otlp_span_t *span, const uint8_t *span_id)
{
	if (!span || !span_id)
		return OTLP_ERR_NULL;
	/* W3C Trace Context §3.1.2: parent-id (span-id) MUST NOT be
	 * all-zero. */
	if (otlp_id_is_all_zero(span_id, OTLP_SPAN_ID_LEN))
		return OTLP_ERR_INVALID_ARGUMENT;
	memcpy(span->span_id, span_id, OTLP_SPAN_ID_LEN);
	return OTLP_OK;
}

otlp_status_t
otlp_span_set_parent_span_id(otlp_span_t *span, const uint8_t *parent)
{
	if (!span)
		return OTLP_ERR_NULL;
	/* parent == NULL clears the parent link. */
	if (!parent)
	{
		memset(span->parent_span_id, 0, OTLP_SPAN_ID_LEN);
		span->has_parent = false;
		return OTLP_OK;
	}
	/* W3C Trace Context §3.1.2: parent-id MUST NOT be all-zero.
	 * Use NULL to clear. */
	if (otlp_id_is_all_zero(parent, OTLP_SPAN_ID_LEN))
		return OTLP_ERR_INVALID_ARGUMENT;
	memcpy(span->parent_span_id, parent, OTLP_SPAN_ID_LEN);
	span->has_parent = true;
	return OTLP_OK;
}

/* ── Timing ───────────────────────────────────────────────────── */

otlp_status_t
otlp_span_set_start_time(otlp_span_t *span, uint64_t unix_nano)
{
	if (!span)
		return OTLP_ERR_NULL;
	span->start_time_unix_nano = unix_nano;
	span->has_start_time = true;
	return OTLP_OK;
}

otlp_status_t
otlp_span_set_end_time(otlp_span_t *span, uint64_t unix_nano)
{
	if (!span)
		return OTLP_ERR_NULL;
	span->end_time_unix_nano = unix_nano;
	span->has_end_time = true;
	return OTLP_OK;
}

otlp_status_t
otlp_span_mark_start(otlp_span_t *span)
{
	if (!span)
		return OTLP_ERR_NULL;
	return otlp_platform_now_unix_nano(&span->start_time_unix_nano) ==
			OTLP_OK
		? (span->has_start_time = true, OTLP_OK)
		: OTLP_ERR_NETWORK;
}

otlp_status_t
otlp_span_mark_end(otlp_span_t *span)
{
	if (!span)
		return OTLP_ERR_NULL;
	return otlp_platform_now_unix_nano(&span->end_time_unix_nano) == OTLP_OK
		? (span->has_end_time = true, OTLP_OK)
		: OTLP_ERR_NETWORK;
}

/* ── Metadata ─────────────────────────────────────────────────── */

otlp_status_t
otlp_span_set_kind(otlp_span_t *span, otlp_span_kind_t kind)
{
	if (!span)
		return OTLP_ERR_NULL;
	span->kind = kind;
	return OTLP_OK;
}

otlp_status_t
otlp_span_set_name(otlp_span_t *span, const char *name)
{
	char *new_name;

	if (!span)
		return OTLP_ERR_NULL;
	if (name && !otlp_str_is_utf8(name))
		return OTLP_ERR_UTF8;
	new_name = otlp_dup_str(name ? name : "");
	if (!new_name)
		return OTLP_ERR_NOMEM;
	otlp_free(span->name);
	span->name = new_name;
	return OTLP_OK;
}

/* ── Attributes ───────────────────────────────────────────────── */

otlp_status_t
otlp_span_set_attribute_string(otlp_span_t *span,
	const char *key,
	const char *value)
{
	otlp_value_t v = { .type = OTLP_VALUE_STRING,
		.v = { .string_val = value ? value : "" } };
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(
		&span->attrs, OTLP_SPAN_MAX_ATTRIBUTES, key, &v);
}


otlp_status_t
otlp_span_set_attribute_int(otlp_span_t *span, const char *key, int64_t value)
{
	otlp_value_t v = { .type = OTLP_VALUE_INT64,
		.v = { .int64_val = value } };
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(
		&span->attrs, OTLP_SPAN_MAX_ATTRIBUTES, key, &v);
}


otlp_status_t
otlp_span_set_attribute_double(otlp_span_t *span, const char *key, double value)
{
	otlp_value_t v = { .type = OTLP_VALUE_DOUBLE,
		.v = { .double_val = value } };
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(
		&span->attrs, OTLP_SPAN_MAX_ATTRIBUTES, key, &v);
}


otlp_status_t
otlp_span_set_attribute_bool(otlp_span_t *span, const char *key, bool value)
{
	otlp_value_t v = { .type = OTLP_VALUE_BOOL,
		.v = { .bool_val = value } };
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(
		&span->attrs, OTLP_SPAN_MAX_ATTRIBUTES, key, &v);
}


otlp_status_t
otlp_span_set_attribute_bytes(otlp_span_t *span,
	const char *key,
	const uint8_t *bytes,
	size_t len)
{
	otlp_value_t v = { .type = OTLP_VALUE_BYTES,
		.v = { .bytes_val = { .data = bytes, .len = len } } };
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(
		&span->attrs, OTLP_SPAN_MAX_ATTRIBUTES, key, &v);
}


otlp_status_t
otlp_span_set_attribute_array(otlp_span_t *span,
	const char *key,
	const otlp_value_t *items,
	size_t n)
{
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set_array(
		&span->attrs, OTLP_SPAN_MAX_ATTRIBUTES, key, items, n);
}


otlp_status_t
otlp_span_set_attribute_kvlist(otlp_span_t *span,
	const char *key,
	const otlp_kv_t *entries,
	size_t n)
{
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set_kvlist(
		&span->attrs, OTLP_SPAN_MAX_ATTRIBUTES, key, entries, n);
}


/* ── Status ───────────────────────────────────────────────────── */

otlp_status_t
otlp_span_set_status(otlp_span_t *span,
	otlp_status_code_t code,
	const char *description)
{
	char *msg_copy = NULL;

	if (!span)
		return OTLP_ERR_NULL;
	if (description && !otlp_str_is_utf8(description))
		return OTLP_ERR_UTF8;
	if (description)
	{
		msg_copy = otlp_dup_str(description);
		if (!msg_copy)
			return OTLP_ERR_NOMEM;
	}
	otlp_free(span->status_message);
	span->status_message = msg_copy;
	span->status_code = code;
	return OTLP_OK;
}

otlp_status_t
otlp_span_set_sampled(otlp_span_t *span, bool sampled)
{
	if (!span)
		return OTLP_ERR_NULL;
	span->sampled = sampled;
	return OTLP_OK;
}

/* ── Deferred OTLP fields ─────────────────────────────────────── */

/* Grow-on-demand slot for the next event/link (4 -> 8 -> ... slots,
 * bounded by the cap). Realloc failure leaves the old array intact.
 * The new slot is zeroed so its attr vec and owned pointers start
 * valid. Returns NULL on OOM. */
static struct otlp_event *
event_grow(otlp_span_t *span)
{
	if (span->n_events == span->cap_events)
	{
		size_t new_cap = span->cap_events
			? span->cap_events * 2
			: (4 < OTLP_SPAN_MAX_EVENTS ? 4 : OTLP_SPAN_MAX_EVENTS);
		struct otlp_event *grown;

		if (new_cap > OTLP_SPAN_MAX_EVENTS)
			new_cap = OTLP_SPAN_MAX_EVENTS;
		grown = otlp_realloc(span->events, new_cap * sizeof(*grown));
		if (!grown)
			return NULL;
		memset(grown + span->cap_events,
			0,
			(new_cap - span->cap_events) * sizeof(*grown));
		span->events = grown;
		span->cap_events = new_cap;
	}
	return &span->events[span->n_events];
}

static struct otlp_link *
link_grow(otlp_span_t *span)
{
	if (span->n_links == span->cap_links)
	{
		size_t new_cap = span->cap_links
			? span->cap_links * 2
			: (4 < OTLP_SPAN_MAX_LINKS ? 4 : OTLP_SPAN_MAX_LINKS);
		struct otlp_link *grown;

		if (new_cap > OTLP_SPAN_MAX_LINKS)
			new_cap = OTLP_SPAN_MAX_LINKS;
		grown = otlp_realloc(span->links, new_cap * sizeof(*grown));
		if (!grown)
			return NULL;
		memset(grown + span->cap_links,
			0,
			(new_cap - span->cap_links) * sizeof(*grown));
		span->links = grown;
		span->cap_links = new_cap;
	}
	return &span->links[span->n_links];
}

otlp_status_t
otlp_span_add_event(otlp_span_t *span,
	const char *name,
	uint64_t time_unix_nano)
{
	struct otlp_event *ev;
	char *name_copy;

	if (!span || !name)
		return OTLP_ERR_NULL;
	if (span->n_events >= OTLP_SPAN_MAX_EVENTS)
		return OTLP_ERR_OVERFLOW;
	if (!otlp_str_is_utf8(name))
		return OTLP_ERR_UTF8;
	name_copy = otlp_dup_str(name);
	if (!name_copy)
		return OTLP_ERR_NOMEM;
	ev = event_grow(span);
	if (!ev)
	{
		otlp_free(name_copy);
		return OTLP_ERR_NOMEM;
	}
	ev->name = name_copy;
	ev->time_unix_nano = time_unix_nano;
	span->n_events++;
	return OTLP_OK;
}

otlp_status_t
otlp_span_add_link(otlp_span_t *span,
	const uint8_t *trace_id,
	const uint8_t *span_id)
{
	struct otlp_link *lk;

	if (!span)
		return OTLP_ERR_NULL;
	if (!trace_id || !span_id)
		return OTLP_ERR_NULL;
	if (span->n_links >= OTLP_SPAN_MAX_LINKS)
		return OTLP_ERR_OVERFLOW;
	lk = link_grow(span);
	if (!lk)
		return OTLP_ERR_NOMEM;
	memcpy(lk->trace_id, trace_id, OTLP_TRACE_ID_LEN);
	memcpy(lk->span_id, span_id, OTLP_SPAN_ID_LEN);
	span->n_links++;
	return OTLP_OK;
}

otlp_status_t
otlp_span_set_trace_state(otlp_span_t *span, const char *trace_state)
{
	char *copy = NULL;

	if (!span)
		return OTLP_ERR_NULL;
	if (trace_state && !otlp_str_is_utf8(trace_state))
		return OTLP_ERR_UTF8;
	if (trace_state)
	{
		copy = otlp_dup_str(trace_state);
		if (!copy)
			return OTLP_ERR_NOMEM;
	}
	otlp_free(span->trace_state);
	span->trace_state = copy;
	return OTLP_OK;
}

otlp_status_t
otlp_span_set_event_attribute_string(otlp_span_t *span,
	const char *key,
	const char *value)
{
	otlp_value_t v = { .type = OTLP_VALUE_STRING,
		.v = { .string_val = value ? value : "" } };
	if (span->n_events == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&span->events[span->n_events - 1].attrs,
		OTLP_EVENT_MAX_ATTRS,
		key,
		&v);
}


otlp_status_t
otlp_span_set_event_attribute_int(otlp_span_t *span,
	const char *key,
	int64_t value)
{
	otlp_value_t v = { .type = OTLP_VALUE_INT64,
		.v = { .int64_val = value } };
	if (span->n_events == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&span->events[span->n_events - 1].attrs,
		OTLP_EVENT_MAX_ATTRS,
		key,
		&v);
}


otlp_status_t
otlp_span_set_event_attribute_double(otlp_span_t *span,
	const char *key,
	double value)
{
	otlp_value_t v = { .type = OTLP_VALUE_DOUBLE,
		.v = { .double_val = value } };
	if (span->n_events == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&span->events[span->n_events - 1].attrs,
		OTLP_EVENT_MAX_ATTRS,
		key,
		&v);
}


otlp_status_t
otlp_span_set_event_attribute_bool(otlp_span_t *span,
	const char *key,
	bool value)
{
	otlp_value_t v = { .type = OTLP_VALUE_BOOL,
		.v = { .bool_val = value } };
	if (span->n_events == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&span->events[span->n_events - 1].attrs,
		OTLP_EVENT_MAX_ATTRS,
		key,
		&v);
}


otlp_status_t
otlp_span_set_event_attribute_bytes(otlp_span_t *span,
	const char *key,
	const uint8_t *bytes,
	size_t len)
{
	otlp_value_t v = { .type = OTLP_VALUE_BYTES,
		.v = { .bytes_val = { .data = bytes, .len = len } } };
	if (span->n_events == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&span->events[span->n_events - 1].attrs,
		OTLP_EVENT_MAX_ATTRS,
		key,
		&v);
}


otlp_status_t
otlp_span_set_link_attribute_string(otlp_span_t *span,
	const char *key,
	const char *value)
{
	otlp_value_t v = { .type = OTLP_VALUE_STRING,
		.v = { .string_val = value ? value : "" } };
	if (span->n_links == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&span->links[span->n_links - 1].attrs,
		OTLP_LINK_MAX_ATTRS,
		key,
		&v);
}


otlp_status_t
otlp_span_set_link_attribute_int(otlp_span_t *span,
	const char *key,
	int64_t value)
{
	otlp_value_t v = { .type = OTLP_VALUE_INT64,
		.v = { .int64_val = value } };
	if (span->n_links == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&span->links[span->n_links - 1].attrs,
		OTLP_LINK_MAX_ATTRS,
		key,
		&v);
}


otlp_status_t
otlp_span_set_link_attribute_double(otlp_span_t *span,
	const char *key,
	double value)
{
	otlp_value_t v = { .type = OTLP_VALUE_DOUBLE,
		.v = { .double_val = value } };
	if (span->n_links == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&span->links[span->n_links - 1].attrs,
		OTLP_LINK_MAX_ATTRS,
		key,
		&v);
}


otlp_status_t
otlp_span_set_link_attribute_bool(otlp_span_t *span,
	const char *key,
	bool value)
{
	otlp_value_t v = { .type = OTLP_VALUE_BOOL,
		.v = { .bool_val = value } };
	if (span->n_links == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&span->links[span->n_links - 1].attrs,
		OTLP_LINK_MAX_ATTRS,
		key,
		&v);
}


otlp_status_t
otlp_span_set_link_attribute_bytes(otlp_span_t *span,
	const char *key,
	const uint8_t *bytes,
	size_t len)
{
	otlp_value_t v = { .type = OTLP_VALUE_BYTES,
		.v = { .bytes_val = { .data = bytes, .len = len } } };
	if (span->n_links == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set(&span->links[span->n_links - 1].attrs,
		OTLP_LINK_MAX_ATTRS,
		key,
		&v);
}


otlp_status_t
otlp_span_set_event_attribute_array(otlp_span_t *span,
	const char *key,
	const otlp_value_t *items,
	size_t n)
{
	if (span->n_events == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set_array(&span->events[span->n_events - 1].attrs,
		OTLP_EVENT_MAX_ATTRS,
		key,
		items,
		n);
}


otlp_status_t
otlp_span_set_event_attribute_kvlist(otlp_span_t *span,
	const char *key,
	const otlp_kv_t *entries,
	size_t n)
{
	if (span->n_events == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set_kvlist(&span->events[span->n_events - 1].attrs,
		OTLP_EVENT_MAX_ATTRS,
		key,
		entries,
		n);
}


otlp_status_t
otlp_span_set_link_attribute_array(otlp_span_t *span,
	const char *key,
	const otlp_value_t *items,
	size_t n)
{
	if (span->n_links == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set_array(&span->links[span->n_links - 1].attrs,
		OTLP_LINK_MAX_ATTRS,
		key,
		items,
		n);
}


otlp_status_t
otlp_span_set_link_attribute_kvlist(otlp_span_t *span,
	const char *key,
	const otlp_kv_t *entries,
	size_t n)
{
	if (span->n_links == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (!span || !key)
		return OTLP_ERR_NULL;
	return otlp_attr_vec_set_kvlist(&span->links[span->n_links - 1].attrs,
		OTLP_LINK_MAX_ATTRS,
		key,
		entries,
		n);
}


/* ── Internal accessors (see span_internal.h) ─────────────────── */

const char *
otlp_span_get_name(const otlp_span_t *span)
{
	return span ? span->name : NULL;
}

const uint8_t *
otlp_span_get_trace_id(const otlp_span_t *span)
{
	return span ? span->trace_id : NULL;
}

const uint8_t *
otlp_span_get_span_id(const otlp_span_t *span)
{
	return span ? span->span_id : NULL;
}

bool
otlp_span_has_parent(const otlp_span_t *span)
{
	return span ? span->has_parent : false;
}

const uint8_t *
otlp_span_get_parent_span_id(const otlp_span_t *span)
{
	return span ? span->parent_span_id : NULL;
}

otlp_span_kind_t
otlp_span_get_kind(const otlp_span_t *span)
{
	return span ? span->kind : OTLP_SPAN_KIND_UNSPECIFIED;
}

bool
otlp_span_has_start_time(const otlp_span_t *span)
{
	return span ? span->has_start_time : false;
}

uint64_t
otlp_span_get_start_time(const otlp_span_t *span)
{
	return span ? span->start_time_unix_nano : 0;
}

bool
otlp_span_has_end_time(const otlp_span_t *span)
{
	return span ? span->has_end_time : false;
}

uint64_t
otlp_span_get_end_time(const otlp_span_t *span)
{
	return span ? span->end_time_unix_nano : 0;
}

otlp_status_code_t
otlp_span_get_status_code(const otlp_span_t *span)
{
	return span ? span->status_code : OTLP_STATUS_CODE_UNSET;
}

const char *
otlp_span_get_status_message(const otlp_span_t *span)
{
	return span ? span->status_message : NULL;
}

bool
otlp_span_is_sampled(const otlp_span_t *span)
{
	return span ? span->sampled : false;
}

const struct otlp_event *
otlp_span_get_events(const otlp_span_t *span, size_t *n_out)
{
	if (!span)
	{
		if (n_out)
			*n_out = 0;
		return NULL;
	}
	if (n_out)
		*n_out = span->n_events;
	return span->events;
}

const struct otlp_link *
otlp_span_get_links(const otlp_span_t *span, size_t *n_out)
{
	if (!span)
	{
		if (n_out)
			*n_out = 0;
		return NULL;
	}
	if (n_out)
		*n_out = span->n_links;
	return span->links;
}

const char *
otlp_span_get_trace_state(const otlp_span_t *span)
{
	return span ? span->trace_state : NULL;
}

size_t
otlp_span_struct_size(void)
{
	return sizeof(struct otlp_span);
}

const struct otlp_attribute *
otlp_span_get_attrs(const otlp_span_t *span, size_t *n_out)
{
	if (!span)
	{
		if (n_out)
			*n_out = 0;
		return NULL;
	}
	if (n_out)
		*n_out = span->attrs.n;
	return span->attrs.items;
}

otlp_span_t *
otlp_span_clone(const otlp_span_t *src)
{
	otlp_span_t *dst;
	const struct otlp_attribute *attrs;
	const struct otlp_event *events;
	const struct otlp_link *links;
	size_t n_attrs, n_events, n_links;
	size_t i;

	if (!src)
		return NULL;
	dst = otlp_span_create(otlp_span_get_name(src));
	if (!dst)
		return NULL;

	memcpy(dst->trace_id, src->trace_id, OTLP_TRACE_ID_LEN);
	memcpy(dst->span_id, src->span_id, OTLP_SPAN_ID_LEN);
	dst->has_parent = src->has_parent;
	if (src->has_parent)
		memcpy(dst->parent_span_id,
			src->parent_span_id,
			OTLP_SPAN_ID_LEN);
	dst->has_start_time = src->has_start_time;
	dst->start_time_unix_nano = src->start_time_unix_nano;
	dst->has_end_time = src->has_end_time;
	dst->end_time_unix_nano = src->end_time_unix_nano;
	dst->kind = src->kind;
	dst->status_code = src->status_code;
	dst->sampled = src->sampled;
	if (src->status_message)
	{
		dst->status_message = otlp_dup_str(src->status_message);
		if (!dst->status_message)
		{
			otlp_span_free(dst);
			return NULL;
		}
	}
	if (src->trace_state)
	{
		dst->trace_state = otlp_dup_str(src->trace_state);
		if (!dst->trace_state)
		{
			otlp_span_free(dst);
			return NULL;
		}
	}

	attrs = otlp_span_get_attrs(src, &n_attrs);
	(void) attrs;
	if (otlp_attr_vec_copy(&dst->attrs, &src->attrs) != OTLP_OK)
	{
		otlp_span_free(dst);
		return NULL;
	}

	events = otlp_span_get_events(src, &n_events);
	for (i = 0; i < n_events; i++)
	{
		otlp_status_t st = otlp_span_add_event(
			dst, events[i].name, events[i].time_unix_nano);
		if (st != OTLP_OK)
		{
			otlp_span_free(dst);
			return NULL;
		}
		/* Copy event attributes (was missing — data loss bug,
		 * fixed v0.5.33); deep-copied via the shared vec helper. */
		if (events[i].attrs.n > 0)
		{
			struct otlp_event *ev = &dst->events[dst->n_events - 1];
			if (otlp_attr_vec_copy(&ev->attrs, &events[i].attrs) !=
				OTLP_OK)
			{
				otlp_span_free(dst);
				return NULL;
			}
		}
	}

	links = otlp_span_get_links(src, &n_links);
	for (i = 0; i < n_links; i++)
	{
		otlp_status_t st = otlp_span_add_link(
			dst, links[i].trace_id, links[i].span_id);
		if (st != OTLP_OK)
		{
			otlp_span_free(dst);
			return NULL;
		}
		/* Copy link attributes (was missing — data loss bug); see
		 * event path. */
		if (links[i].attrs.n > 0)
		{
			struct otlp_link *lnk = &dst->links[dst->n_links - 1];
			if (otlp_attr_vec_copy(&lnk->attrs, &links[i].attrs) !=
				OTLP_OK)
			{
				otlp_span_free(dst);
				return NULL;
			}
		}
	}
	return dst;
}
