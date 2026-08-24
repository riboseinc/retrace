/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Protobuf wire encoder — see src/protobuf_encode.h for the API.
 *
 * Hand-rolled for the four wire types OTLP uses (varint, fixed64,
 * fixed32, length-delimited). No third-party protobuf library.
 *
 * Memory model: otlp_pb_buf uses SBO (small-buffer optimisation).
 * _init points data at the inline 64-byte buffer — zero malloc for
 * small messages. _reserve switches to heap (realloc) when the
 * message exceeds the inline size. _free only frees if heap-owned.
 * All public functions are atomic on failure: a partial write
 * leaves the buf unchanged (len advances only after success).
 */
#include "protobuf_encode.h"
#include "internal_util.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* ── Internal helpers ─────────────────────────────────────────── */

/* Ensure buf has room for `additional` more bytes beyond buf->len.
 * Starts in SBO (inline 64 bytes); switches to heap on overflow. */
static otlp_status_t
buf_reserve(struct otlp_pb_buf *buf, size_t additional)
{
	size_t   new_cap;
	size_t   need;
	uint8_t *p;

	if (!buf)
		return OTLP_ERR_NULL;
	if (additional > SIZE_MAX - buf->len)
		return OTLP_ERR_OVERFLOW;
	need = buf->len + additional;
	if (need <= buf->cap)
		return OTLP_OK;

	new_cap = buf->cap ? buf->cap : OTLP_PB_SBO_SIZE;
	while (new_cap < need)
	{
		if (new_cap > SIZE_MAX / 2)
			return OTLP_ERR_OVERFLOW;
		new_cap *= 2;
	}

	if (!buf->owns_heap && buf->cap > 0)
	{
		/* Transitioning from SBO to heap: allocate new buffer
		 * and copy inline contents. */
		p = otlp_malloc(new_cap);
		if (!p)
			return OTLP_ERR_NOMEM;
		memcpy(p, buf->data, buf->len);
	}
	else
	{
		p = otlp_realloc(buf->data, new_cap);
		if (!p)
			return OTLP_ERR_NOMEM;
	}
	buf->data	    = p;
	buf->cap	    = new_cap;
	buf->owns_heap = true;
	return OTLP_OK;
}

/* Append `len` bytes from `data`. data may be NULL only when len==0. */
static otlp_status_t
buf_append(struct otlp_pb_buf *buf, const uint8_t *data, size_t len)
{
	otlp_status_t st;

	if (len == 0)
		return OTLP_OK;
	if (!data)
		return OTLP_ERR_NULL;
	st = buf_reserve(buf, len);
	if (st != OTLP_OK)
		return st;
	memcpy(buf->data + buf->len, data, len);
	buf->len += len;
	return OTLP_OK;
}

/* ── Buffer lifecycle ─────────────────────────────────────────── */

otlp_status_t
otlp_pb_buf_init(struct otlp_pb_buf *buf, size_t initial_cap)
{
	if (!buf)
		return OTLP_ERR_NULL;
	/* SBO: point at the inline buffer by default. */
	buf->data	    = buf->sbo;
	buf->len	    = 0;
	buf->cap	    = OTLP_PB_SBO_SIZE;
	buf->owns_heap = false;

	if (initial_cap > OTLP_PB_SBO_SIZE)
	{
		/* Caller asked for more than SBO can hold — malloc. */
		buf->data = otlp_malloc(initial_cap);
		if (!buf->data)
		{
			buf->data	    = buf->sbo;
			buf->cap	    = OTLP_PB_SBO_SIZE;
			return OTLP_ERR_NOMEM;
		}
		buf->cap	    = initial_cap;
		buf->owns_heap = true;
	}
	return OTLP_OK;
}

void
otlp_pb_buf_free(struct otlp_pb_buf *buf)
{
	if (!buf)
		return;
	if (buf->owns_heap)
		otlp_free(buf->data);
	buf->data	    = NULL;
	buf->len	    = 0;
	buf->cap	    = 0;
	buf->owns_heap = false;
}

void
otlp_pb_buf_reset(struct otlp_pb_buf *buf)
{
	if (!buf)
		return;
	buf->len = 0;
}

/* ── Low-level wire encoders ────────────────────────────────────
 *
 * All encoders append to buf. None of them free or reset buf. On
 * failure they return without modifying buf->len (the reserve may
 * have grown buf->cap, but that is harmless — cap only grows).
 */

otlp_status_t
otlp_pb_varint(struct otlp_pb_buf *buf, uint64_t v)
{
	uint8_t tmp[10];
	size_t n = 0;

	do
	{
		tmp[n++] = (uint8_t) ((v & 0x7F) | 0x80);
		v >>= 7;
	} while (v != 0 && n < sizeof(tmp));

	/* Clear continuation bit on the last byte. */
	tmp[n - 1] &= 0x7F;
	return buf_append(buf, tmp, n);
}

otlp_status_t
otlp_pb_fixed64(struct otlp_pb_buf *buf, uint64_t v)
{
	uint8_t tmp[8];

	/* Little-endian, no platform dependency (shifts, not union). */
	for (int i = 0; i < 8; i++)
		tmp[i] = (uint8_t) (v >> (i * 8));
	return buf_append(buf, tmp, sizeof(tmp));
}

otlp_status_t
otlp_pb_fixed32(struct otlp_pb_buf *buf, uint32_t v)
{
	uint8_t tmp[4];

	for (int i = 0; i < 4; i++)
		tmp[i] = (uint8_t) (v >> (i * 8));
	return buf_append(buf, tmp, sizeof(tmp));
}

otlp_status_t
otlp_pb_bytes(struct otlp_pb_buf *buf, const uint8_t *data, size_t len)
{
	otlp_status_t st;

	/* Length prefix (varint) then payload. */
	st = otlp_pb_varint(buf, (uint64_t) len);
	if (st != OTLP_OK)
		return st;
	if (len == 0)
		return OTLP_OK;
	return buf_append(buf, data, len);
}

otlp_status_t
otlp_pb_string(struct otlp_pb_buf *buf, const char *str)
{
	size_t len = str ? strlen(str) : 0;

	return otlp_pb_bytes(buf, (const uint8_t *) str, len);
}

otlp_status_t
otlp_pb_tag(struct otlp_pb_buf *buf, uint32_t field_number, int wire_type)
{
	uint64_t key;

	/* field_number 0 is reserved by protobuf. Wire types 3 (start
	 * group) and 4 (end group) are deprecated and not used by OTLP. */
	if (field_number == 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (wire_type != OTLP_PB_WIRE_VARINT &&
		wire_type != OTLP_PB_WIRE_FIXED64 &&
		wire_type != OTLP_PB_WIRE_LEN &&
		wire_type != OTLP_PB_WIRE_FIXED32)
		return OTLP_ERR_INVALID_ARGUMENT;

	key = ((uint64_t) field_number << 3) | (uint64_t) (uint32_t) wire_type;
	return otlp_pb_varint(buf, key);
}

/* ── Typed field helpers ────────────────────────────────────────
 *
 * Each helper emits tag + value. Skip emission when value is the
 * type's zero value (protobuf3 default-omission semantics). Callers
 * that must emit a default value explicitly should call otlp_pb_tag
 * + otlp_pb_varint(0) (or _bytes(buf, "", 0)) directly.
 *
 * field_message is the exception: it emits even for a zero-length
 * sub-message only if explicitly asked; the convention here is to
 * skip empty sub-messages, matching protobuf3.
 */

otlp_status_t
otlp_pb_field_varint(struct otlp_pb_buf *buf,
	uint32_t field_num,
	uint64_t value)
{
	otlp_status_t st;

	if (value == 0)
		return OTLP_OK;
	st = otlp_pb_tag(buf, field_num, OTLP_PB_WIRE_VARINT);
	if (st != OTLP_OK)
		return st;
	return otlp_pb_varint(buf, value);
}

otlp_status_t
otlp_pb_field_fixed64(struct otlp_pb_buf *buf,
	uint32_t field_num,
	uint64_t value)
{
	otlp_status_t st;

	if (value == 0)
		return OTLP_OK;
	st = otlp_pb_tag(buf, field_num, OTLP_PB_WIRE_FIXED64);
	if (st != OTLP_OK)
		return st;
	return otlp_pb_fixed64(buf, value);
}

otlp_status_t
otlp_pb_field_fixed32(struct otlp_pb_buf *buf,
	uint32_t field_num,
	uint32_t value)
{
	otlp_status_t st;

	if (value == 0)
		return OTLP_OK;
	st = otlp_pb_tag(buf, field_num, OTLP_PB_WIRE_FIXED32);
	if (st != OTLP_OK)
		return st;
	return otlp_pb_fixed32(buf, value);
}

otlp_status_t
otlp_pb_field_string(struct otlp_pb_buf *buf,
	uint32_t field_num,
	const char *str)
{
	otlp_status_t st;
	size_t len;

	/* Empty strings are omitted (protobuf3 default). */
	if (!str || str[0] == '\0')
		return OTLP_OK;
	len = strlen(str);
	st = otlp_pb_tag(buf, field_num, OTLP_PB_WIRE_LEN);
	if (st != OTLP_OK)
		return st;
	return otlp_pb_bytes(buf, (const uint8_t *) str, len);
}

otlp_status_t
otlp_pb_field_bytes(struct otlp_pb_buf *buf,
	uint32_t field_num,
	const uint8_t *data,
	size_t len)
{
	otlp_status_t st;

	/* Empty bytes are omitted (protobuf3 default). Callers needing
	 * to emit explicit empty bytes should use tag + varint(0). */
	if (len == 0)
		return OTLP_OK;
	st = otlp_pb_tag(buf, field_num, OTLP_PB_WIRE_LEN);
	if (st != OTLP_OK)
		return st;
	return otlp_pb_bytes(buf, data, len);
}

otlp_status_t
otlp_pb_field_message(struct otlp_pb_buf *buf,
	uint32_t field_num,
	const uint8_t *data,
	size_t len)
{
	otlp_status_t st;

	/* Empty sub-messages are omitted (protobuf3 default). */
	if (len == 0)
		return OTLP_OK;
	st = otlp_pb_tag(buf, field_num, OTLP_PB_WIRE_LEN);
	if (st != OTLP_OK)
		return st;
	st = otlp_pb_varint(buf, (uint64_t) len);
	if (st != OTLP_OK)
		return st;
	return buf_append(buf, data, len);
}
