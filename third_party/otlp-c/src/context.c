/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * SpanContext propagation. See include/otlp-c/context.h.
 *
 * Uses the traceparent format from w3c.c. The carrier abstraction
 * (callback-based setter/getter) keeps the library transport-
 * agnostic: the caller decides whether the carrier is an HTTP
 * header map, gRPC metadata, a message attribute table, or
 * something else entirely.
 */
#include <otlp-c/context.h>
#include <otlp-c/span.h>
#include <otlp-c/status.h>
#include <otlp-c/w3c.h>

#include "span_internal.h"

#include <stdint.h>
#include <string.h>

const char OTLP_CONTEXT_TRACEPARENT_HEADER[] = "traceparent";
const char OTLP_CONTEXT_TRACESTATE_HEADER[] = "tracestate";
const char OTLP_CONTEXT_BAGGAGE_HEADER[] = "baggage";

/* Propagated header values may only contain printable ASCII
 * (W3C tracestate/baggage grammars; HTTP header values). Anything
 * else is rejected: CR/LF would split into a new header line when
 * injected (CWE-93, header injection via propagated context) and
 * other control bytes would produce invalid outgoing headers. */
static bool
contains_control(const char *s)
{
	for (; *s != '\0'; s++)
	{
		unsigned char c = (unsigned char) *s;

		if (c < 0x20 || c == 0x7f)
			return true;
	}
	return false;
}

otlp_context_t
otlp_context_from_span(const otlp_span_t *span)
{
	otlp_context_t ctx = { 0 };
	const uint8_t *trace_id;
	const uint8_t *span_id;

	if (!span)
	{
		ctx.has_context = false;
		return ctx;
	}
	trace_id = otlp_span_get_trace_id(span);
	span_id = otlp_span_get_span_id(span);
	if (!trace_id || !span_id)
	{
		ctx.has_context = false;
		return ctx;
	}
	memcpy(ctx.trace_id, trace_id, OTLP_TRACE_ID_LEN);
	memcpy(ctx.span_id, span_id, OTLP_SPAN_ID_LEN);
	ctx.sampled = otlp_span_is_sampled(span);
	ctx.has_context = true;
	return ctx;
}

otlp_status_t
otlp_context_inject(otlp_context_t ctx,
	otlp_carrier_set_fn set,
	void *carrier_ctx)
{
	char buf[OTLP_TRACEPARENT_BUF_SIZE];
	otlp_status_t st;

	if (!set)
		return OTLP_ERR_NULL;
	if (!ctx.has_context)
		return OTLP_ERR_INVALID_ARGUMENT;

	/* Build the traceparent value from raw IDs via the shared
	 * primitive in w3c.c (DRY: no inlined hex formatting here). */
	st = otlp_traceparent_format_raw(
		ctx.trace_id, ctx.span_id, ctx.sampled, buf, sizeof(buf), NULL);
	if (st != OTLP_OK)
		return st;

	/* Emit tracestate if present (non-empty). */
	if (ctx.tracestate[0])
	{
		otlp_status_t ts_st;

		ts_st = set(carrier_ctx,
			OTLP_CONTEXT_TRACESTATE_HEADER,
			ctx.tracestate);
		if (ts_st != OTLP_OK)
			return ts_st;
	}

	/* Emit baggage if present (non-empty). */
	if (ctx.baggage[0])
	{
		otlp_status_t bg_st;

		bg_st = set(
			carrier_ctx, OTLP_CONTEXT_BAGGAGE_HEADER, ctx.baggage);
		if (bg_st != OTLP_OK)
			return bg_st;
	}

	return set(carrier_ctx, OTLP_CONTEXT_TRACEPARENT_HEADER, buf);
}

otlp_context_t
otlp_context_extract(otlp_carrier_get_fn get, void *carrier_ctx)
{
	otlp_context_t ctx = { 0 };
	const char *header;
	uint8_t flags;

	if (!get)
	{
		ctx.has_context = false;
		return ctx;
	}
	header = get(carrier_ctx, OTLP_CONTEXT_TRACEPARENT_HEADER);
	if (!header)
	{
		ctx.has_context = false;
		return ctx;
	}

	/* Reuse the W3C parser. */
	if (otlp_traceparent_parse(header, ctx.trace_id, ctx.span_id, &flags) !=
		OTLP_OK)
	{
		ctx.has_context = false;
		return ctx;
	}
	ctx.sampled = (flags & 0x01) != 0;
	ctx.has_context = true;

	/* Extract tracestate if present. Reject values containing
	 * control bytes (header injection defense; see
	 * contains_control). */
	{
		const char *ts =
			get(carrier_ctx, OTLP_CONTEXT_TRACESTATE_HEADER);

		if (ts && ts[0] && !contains_control(ts))
		{
			size_t len = strlen(ts);

			if (len >= OTLP_CONTEXT_TRACESTATE_MAX)
				len = OTLP_CONTEXT_TRACESTATE_MAX - 1;
			memcpy(ctx.tracestate, ts, len);
			ctx.tracestate[len] = '\0';
		}
	}

	/* Extract baggage if present. Reject values containing
	 * control bytes (see contains_control). */
	{
		const char *bg = get(carrier_ctx, OTLP_CONTEXT_BAGGAGE_HEADER);

		if (bg && bg[0] && !contains_control(bg))
		{
			size_t len = strlen(bg);

			if (len >= OTLP_CONTEXT_BAGGAGE_MAX)
				len = OTLP_CONTEXT_BAGGAGE_MAX - 1;
			memcpy(ctx.baggage, bg, len);
			ctx.baggage[len] = '\0';
		}
	}
	return ctx;
}
