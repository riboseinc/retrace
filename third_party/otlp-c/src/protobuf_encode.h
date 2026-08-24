/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Internal Protobuf encoder header. The next agent fills in the
 * definitions; the public surface here is internal-only (not
 * exposed in include/).
 *
 * Wire types:
 *   0 = varint    (int32, int64, uint32, uint64, bool, enum)
 *   1 = fixed64   (fixed64, sfixed64, double)
 *   2 = length-delimited (string, bytes, embedded messages,
 *                         packed repeated fields)
 *   5 = fixed32   (fixed32, sfixed32, float)
 *
 * Key encoding: (field_number << 3) | wire_type, as a varint.
 */
#ifndef OTLP_C_PROTOBUF_ENCODE_H
#define OTLP_C_PROTOBUF_ENCODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <otlp-c/status.h>

#define OTLP_PB_WIRE_VARINT 0
#define OTLP_PB_WIRE_FIXED64 1
#define OTLP_PB_WIRE_LEN 2
#define OTLP_PB_WIRE_FIXED32 5

/* Encoder buffer. Grows as fields are appended. Uses small-buffer
 * optimisation (SBO): the first 64 bytes are inline (no malloc).
 * Most OTLP sub-messages (Status, KeyValue, AnyValue) fit in 64
 * bytes, so the encoder mallocs zero times per batch for typical
 * attribute counts. */
#define OTLP_PB_SBO_SIZE 192

struct otlp_pb_buf
{
	uint8_t *data;
	size_t len;
	size_t cap;
	uint8_t sbo[OTLP_PB_SBO_SIZE];
	bool owns_heap;
};

/* Initialize a buf. Returns OTLP_OK or OTLP_ERR_NOMEM. */
otlp_status_t
otlp_pb_buf_init(struct otlp_pb_buf *buf, size_t initial_cap);

/* Free a buf's data. */
void
otlp_pb_buf_free(struct otlp_pb_buf *buf);

/* Reset len to 0 without freeing memory. */
void
otlp_pb_buf_reset(struct otlp_pb_buf *buf);

/* ── Low-level encoders ──────────────────────────────────────────
 */

/* Encode a varint (wire type 0). */
otlp_status_t
otlp_pb_varint(struct otlp_pb_buf *buf, uint64_t value);

/* Encode a fixed64 (wire type 1). */
otlp_status_t
otlp_pb_fixed64(struct otlp_pb_buf *buf, uint64_t value);

/* Encode a fixed32 (wire type 5). */
otlp_status_t
otlp_pb_fixed32(struct otlp_pb_buf *buf, uint32_t value);

/* Encode length-delimited bytes (wire type 2). */
otlp_status_t
otlp_pb_bytes(struct otlp_pb_buf *buf, const uint8_t *data, size_t len);

/* Encode a string (wire type 2; convenience wrapper). */
otlp_status_t
otlp_pb_string(struct otlp_pb_buf *buf, const char *str);

/* Encode a key (tag). `field_number` is the .proto field number;
 * `wire_type` is one of OTLP_PB_WIRE_*. */
otlp_status_t
otlp_pb_tag(struct otlp_pb_buf *buf, uint32_t field_number, int wire_type);

/* ── Typed field encoders (key + value, convenience) ────────────
 *
 * These wrap the tag + value pair. Skip if value is the type's
 * zero value (Protobuf default).
 */

otlp_status_t
otlp_pb_field_varint(struct otlp_pb_buf *buf,
	uint32_t field_num,
	uint64_t value);

otlp_status_t
otlp_pb_field_fixed64(struct otlp_pb_buf *buf,
	uint32_t field_num,
	uint64_t value);

otlp_status_t
otlp_pb_field_fixed32(struct otlp_pb_buf *buf,
	uint32_t field_num,
	uint32_t value);

otlp_status_t
otlp_pb_field_string(struct otlp_pb_buf *buf,
	uint32_t field_num,
	const char *str);

otlp_status_t
otlp_pb_field_bytes(struct otlp_pb_buf *buf,
	uint32_t field_num,
	const uint8_t *data,
	size_t len);

/* Embed a sub-message. The caller passes the encoded sub-message
 * as a (data, len) pair; this function emits tag + length + data. */
otlp_status_t
otlp_pb_field_message(struct otlp_pb_buf *buf,
	uint32_t field_num,
	const uint8_t *data,
	size_t len);

#endif
