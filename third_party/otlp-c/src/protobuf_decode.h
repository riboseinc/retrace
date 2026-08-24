/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Minimal protobuf wire-format reader — the decode counterpart of
 * protobuf_encode.h. Scope: walking well-formed (or malformed, but
 * bounded) length-delimited messages to extract specific fields.
 * The library never decodes arbitrary user protobuf; the only
 * consumer today is the Export*ServiceResponse PartialSuccess
 * decode in exporter_otel.c.
 *
 * Every primitive is bounds-checked: a malformed buffer (truncated
 * varint, length past end-of-buffer, reserved wire types) makes the
 * call return false, never read out of bounds.
 */
#ifndef OTLP_C_PROTOBUF_DECODE_H
#define OTLP_C_PROTOBUF_DECODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protobuf_encode.h" /* OTLP_PB_WIRE_* constants */

struct otlp_pb_reader
{
	const uint8_t *buf;
	size_t len;
	size_t pos;
};

static inline void
otlp_pb_reader_init(struct otlp_pb_reader *r, const uint8_t *buf, size_t len)
{
	r->buf = buf;
	r->len = len;
	r->pos = 0;
}

/* Read the next field key. Returns false at clean end-of-buffer or
 * on a malformed key. `field` and `wire_type` are set on success. */
bool
otlp_pb_read_key(struct otlp_pb_reader *r, uint32_t *field, int *wire_type);

/* Read the value of the CURRENT field as a varint (wire type 0).
 * False on wrong wire type or truncated varint. */
bool
otlp_pb_read_varint(struct otlp_pb_reader *r, uint64_t *out);

/* Read the value of the CURRENT field as length-delimited bytes
 * (wire type 2). `data` points INTO the reader's buffer (no copy);
 * valid while the buffer is. False on wrong wire type or a length
 * past end-of-buffer. */
bool
otlp_pb_read_len(struct otlp_pb_reader *r, const uint8_t **data, size_t *len);

/* Skip the value of the CURRENT field. Handles every wire type the
 * encoder can emit plus unknown fields. False on groups (3/4,
 * reserved and never emitted by OTLP) or truncated values. */
bool
otlp_pb_skip(struct otlp_pb_reader *r, int wire_type);

/* Read the CURRENT field as a 4/8-byte fixed little-endian value
 * (wire types 5/1). Bounds-checked like every other primitive. */
bool
otlp_pb_read_fixed32(struct otlp_pb_reader *r, uint32_t *out);

bool
otlp_pb_read_fixed64(struct otlp_pb_reader *r, uint64_t *out);

#endif
