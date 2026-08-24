/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * SpanContext — propagates trace context across process boundaries.
 *
 * A SpanContext is an immutable snapshot of the identifying fields
 * of a span (trace-id, span-id, sampled flag). It is what gets
 * serialized into a carrier (HTTP headers, gRPC metadata, message
 * attributes) on the way out of a process, and deserialized on the
 * way in.
 *
 * The library uses the W3C Trace Context traceparent header format
 * (https://www.w3.org/TR/trace-context/) for the wire encoding,
 * implemented in w3c.h. The carrier abstraction is a pair of
 * callbacks so the caller can plug in any transport — HTTP header
 * map, gRPC metadata bin, AMQP message attribute table, etc.
 *
 * The library does NOT maintain thread-local context or implicit
 * request-scoped state. Callers thread the context explicitly.
 */
#ifndef OTLP_C_CONTEXT_H
#define OTLP_C_CONTEXT_H

#include <otlp-c/span.h>
#include <otlp-c/status.h>
#include <otlp-c/visibility.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Immutable trace context snapshot. Pass-by-value.
 *
 * The `tracestate` field carries the raw W3C tracestate header value
 * (key=value,key=value,...). The library does NOT parse it — the
 * caller is responsible for formatting on inject and parsing on
 * extract. Empty string means "no tracestate".
 *
 * The `baggage` field carries the raw W3C baggage header value
 * (key1=value1,key2=value2;prop=val). Same opaque-string contract
 * as tracestate: the library propagates it without parsing. Empty
 * string means "no baggage". See:
 * https://www.w3.org/TR/baggage/
 *
 * OTLP_C_CONTEXT_BAGGAGE_MAX is set to 2048 bytes. The W3C spec
 * recommends implementations support at least 8192; callers with
 * larger baggage should split entries or use a side-channel. */
#define OTLP_CONTEXT_TRACESTATE_MAX 512
#define OTLP_CONTEXT_BAGGAGE_MAX 2048

typedef struct otlp_context {
	uint8_t trace_id[OTLP_TRACE_ID_LEN];
	uint8_t span_id[OTLP_SPAN_ID_LEN];
	bool    has_context;  /* false if extracted from an empty/invalid carrier */
	bool    sampled;      /* W3C trace-flags bit 0 */
	char    tracestate[OTLP_CONTEXT_TRACESTATE_MAX];
	char    baggage[OTLP_CONTEXT_BAGGAGE_MAX];
} otlp_context_t;

/* Carrier abstraction. The library calls `set` to write a header
 * during inject, and `get` to read a header during extract. The
 * carrier_ctx is opaque to the library — it's whatever state the
 * caller needs to identify their transport (a header map pointer,
 * a curl handle, a gRPC metadata struct, etc.).
 *
 * `set` returns OTLP_OK on success, an OTLP_ERR_* on failure.
 * `get` returns the header value (NULL if absent).
 */
typedef otlp_status_t (*otlp_carrier_set_fn)(void       *carrier_ctx,
					     const char *key,
					     const char *value);
typedef const char *(*otlp_carrier_get_fn)(void       *carrier_ctx,
					    const char *key);

/* Header name constants (UTF-8, NUL-terminated). */
OTLP_C_EXPORT extern const char OTLP_CONTEXT_TRACEPARENT_HEADER[];
OTLP_C_EXPORT extern const char OTLP_CONTEXT_TRACESTATE_HEADER[];
OTLP_C_EXPORT extern const char OTLP_CONTEXT_BAGGAGE_HEADER[];

/* Build a context from a span's identity fields.
 *
 * Returns a context with has_context=true. If `span` is NULL the
 * returned context has has_context=false. */
OTLP_C_EXPORT
otlp_context_t otlp_context_from_span(const otlp_span_t *span);

/* Inject a context into a carrier using the W3C traceparent header.
 *
 * Writes up to three headers:
 *   - "traceparent" (always, when ctx.has_context)
 *   - "tracestate"  (when ctx.tracestate is non-empty)
 *   - "baggage"     (when ctx.baggage is non-empty)
 *
 * Returns OTLP_OK on success, OTLP_ERR_NULL if set/carrier_ctx is
 * NULL, OTLP_ERR_INVALID_ARGUMENT if ctx.has_context is false, or
 * whatever `set` returns on failure. */
OTLP_C_EXPORT
otlp_status_t otlp_context_inject(otlp_context_t     ctx,
				  otlp_carrier_set_fn set,
				  void	      *carrier_ctx);

/* Extract a context from a carrier by reading the traceparent header.
 *
 * Reads up to three headers:
 *   - "traceparent" (required; absent → has_context=false)
 *   - "tracestate"  (optional; copied verbatim into ctx.tracestate)
 *   - "baggage"     (optional; copied verbatim into ctx.baggage)
 *
 * Returns a context with has_context=true on successful parse, or
 * has_context=false if the header is absent or invalid. Never
 * returns an error code — callers check the returned context. */
OTLP_C_EXPORT
otlp_context_t otlp_context_extract(otlp_carrier_get_fn get,
				    void	       *carrier_ctx);

#ifdef __cplusplus
}
#endif

#endif
