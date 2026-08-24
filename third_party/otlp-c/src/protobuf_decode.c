/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Protobuf wire-format reader implementation. See protobuf_decode.h.
 *
 * Malformed-input policy: every out-of-bounds condition returns
 * false. No partial reads: a caller that gets false must treat the
 * whole walk as failed.
 */
#include "protobuf_decode.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

bool
otlp_pb_read_key(struct otlp_pb_reader *r, uint32_t *field, int *wire_type)
{
	uint64_t key = 0;
	int shift = 0;

	while (r->pos < r->len)
	{
		uint8_t b = r->buf[r->pos++];

		key |= (uint64_t)(b & 0x7f) << shift;
		if ((b & 0x80) == 0)
		{
			*field = (uint32_t)(key >> 3);
			*wire_type = (int) (key & 7);
			/* Field number 0 is invalid on the wire
			 * (protobuf spec); reject rather than let a
			 * garbage key alias a real field. */
			return *field != 0;
		}
		shift += 7;
		if (shift >= 64)
			return false; /* varint longer than 10 bytes */
	}
	return false; /* truncated key */
}

bool
otlp_pb_read_varint(struct otlp_pb_reader *r, uint64_t *out)
{
	uint64_t v = 0;
	int shift = 0;

	while (r->pos < r->len)
	{
		uint8_t b = r->buf[r->pos++];

		v |= (uint64_t)(b & 0x7f) << shift;
		if ((b & 0x80) == 0)
		{
			*out = v;
			return true;
		}
		shift += 7;
		if (shift >= 64)
			return false;
	}
	return false; /* truncated varint */
}

bool
otlp_pb_read_len(struct otlp_pb_reader *r, const uint8_t **data, size_t *len)
{
	uint64_t v = 0;

	if (!otlp_pb_read_varint(r, &v))
		return false;
	/* Length must fit the remaining buffer (and size_t). */
	if (v > (uint64_t)(r->len - r->pos))
		return false;
	*data = r->buf + r->pos;
	*len = (size_t) v;
	r->pos += (size_t) v;
	return true;
}

bool
otlp_pb_skip(struct otlp_pb_reader *r, int wire_type)
{
	switch (wire_type)
	{
		case OTLP_PB_WIRE_VARINT:
		{
			uint64_t v;

			return otlp_pb_read_varint(r, &v);
		}
		case OTLP_PB_WIRE_FIXED64:
			if (r->len - r->pos < 8)
				return false;
			r->pos += 8;
			return true;
		case OTLP_PB_WIRE_LEN:
		{
			const uint8_t *data;
			size_t len;

			return otlp_pb_read_len(r, &data, &len);
		}
		case OTLP_PB_WIRE_FIXED32:
			if (r->len - r->pos < 4)
				return false;
			r->pos += 4;
			return true;
		default:
			/* Wire types 3/4 (groups) are deprecated and
			 * never emitted by OTLP; 6/7 are invalid. */
			return false;
	}
}

bool
otlp_pb_read_fixed32(struct otlp_pb_reader *r, uint32_t *out)
{
	if (r->len - r->pos < 4)
		return false;
	memcpy(out, r->buf + r->pos, 4);
	r->pos += 4;
	return true;
}

bool
otlp_pb_read_fixed64(struct otlp_pb_reader *r, uint64_t *out)
{
	if (r->len - r->pos < 8)
		return false;
	memcpy(out, r->buf + r->pos, 8);
	r->pos += 8;
	return true;
}
