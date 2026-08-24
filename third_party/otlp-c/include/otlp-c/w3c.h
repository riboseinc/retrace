/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * W3C Trace Context utilities — traceparent header format/parse.
 *
 * These are pure functions that bridge between otlp-c's internal
 * span types and the W3C Trace Context wire format. The caller
 * owns the HTTP header insertion/extraction; the library provides
 * the formatting primitives.
 *
 * traceparent format (W3C Trace Context Level 1):
 *   version "-" trace-id "-" parent-id "-" trace-flags
 *   00       -  32 hex     -  16 hex     -  2 hex
 *
 * Example:
 *   00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01
 *
 * The library does NOT manage context lifecycle, thread-local
 * storage, or request-scoped state. The caller decides where to
 * inject/extract the header (HTTP, gRPC metadata, message queue,
 * shared memory, etc.).
 */
#ifndef OTLP_C_W3C_H
#define OTLP_C_W3C_H

#include <otlp-c/span.h>
#include <otlp-c/status.h>
#include <otlp-c/visibility.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimum buffer size for otlp_traceparent_format (55 chars + NUL). */
#define OTLP_TRACEPARENT_LEN 55
#define OTLP_TRACEPARENT_BUF_SIZE (OTLP_TRACEPARENT_LEN + 1)

/* Format raw trace-id + span-id byte arrays as a W3C traceparent
 * header. The primitive that otlp_traceparent_format() (which takes
 * a span) builds on; useful when the caller has raw bytes — e.g.
 * from an otlp_context_t value — rather than a span pointer.
 *
 * `sampled` controls the trace-flags byte (bit 0).
 * `buf` must be at least OTLP_TRACEPARENT_BUF_SIZE bytes.
 * `out_len` receives the number of chars written (excluding NUL).
 *
 * Returns OTLP_OK, OTLP_ERR_NULL, or OTLP_ERR_OVERFLOW. */
OTLP_C_EXPORT
otlp_status_t otlp_traceparent_format_raw(const uint8_t trace_id[16],
					   const uint8_t span_id[8],
					   bool sampled,
					   char *buf,
					   size_t cap,
					   size_t *out_len);

/* Format the span's trace_id + span_id as a W3C traceparent header.
 *
 * Convenience wrapper around otlp_traceparent_format_raw() that
 * extracts the IDs from a span pointer.
 *
 * `sampled` controls the trace-flags byte (bit 0).
 * `buf` must be at least OTLP_TRACEPARENT_BUF_SIZE bytes.
 * `out_len` receives the number of chars written (excluding NUL).
 *
 * Returns OTLP_OK, OTLP_ERR_NULL, or OTLP_ERR_OVERFLOW. */
OTLP_C_EXPORT
otlp_status_t otlp_traceparent_format(const otlp_span_t *span,
				      bool sampled,
				      char *buf,
				      size_t cap,
				      size_t *out_len);

/* Parse a W3C traceparent header into raw byte arrays.
 *
 * Validates format, hex encoding, and rejects all-zero IDs per
 * W3C spec. `flags` receives the trace-flags byte (bit 0 = sampled).
 *
 * Returns OTLP_OK, OTLP_ERR_NULL, or OTLP_ERR_INVALID_ARGUMENT. */
OTLP_C_EXPORT
otlp_status_t otlp_traceparent_parse(const char *header,
				     uint8_t trace_id[16],
				     uint8_t span_id[8],
				     uint8_t *flags);

#ifdef __cplusplus
}
#endif

#endif
