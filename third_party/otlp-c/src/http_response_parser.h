/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * HTTP/1.1 response wire-format parser.
 *
 * A pure byte-in/verdict-out module: the caller owns the buffer,
 * feeds accumulated response bytes, and gets one of three results.
 * No sockets, no clocks, no allocation — everything here is a pure
 * function of (buf, len, at_eof). The socket state machine in
 * http_client.c is a thin adapter over this module; byte-fixture
 * tests are the second (see tests/unit/test_unit_http_response_parser.c
 * and the pure response fuzz in tests/property/test_property_fuzz.c).
 */
#ifndef OTLP_HTTP_RESPONSE_PARSER_H
#define OTLP_HTTP_RESPONSE_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Response size ceiling — shared by the parser (Content-Length and
 * chunk-size rejection) and the socket layer (buffer growth cap). */
#define OTLP_HTTP_RESP_MAX (64 * 1024) /* collector bodies are small */

/* Parsed response fields. `body` points INTO the caller's buffer
 * (zero copy) and stays valid until the buffer is freed or reused. */
struct otlp_http_resp
{
	int http_status;
	uint32_t retry_after_ms; /* Retry-After header, ms (0 = absent) */
	bool keepalive_eligible; /* version default + Connection header */
	const uint8_t *body; /* into buf */
	size_t body_len;
};

/* Parse an HTTP/1.1 response from buf.
 *
 * buf is NOT const: a Transfer-Encoding: chunked body is decoded
 * IN PLACE (framing compacted over; decoded bytes never clobber
 * unread ones). Returns:
 *   1 — complete; *out filled in (body points into buf)
 *   0 — incomplete; need more bytes (or, with no Content-Length,
 *       at_eof before the body can be declared complete)
 *  -1 — malformed (bad status line, TE+CL or duplicate-CL
 *       smuggling vectors, undecodable framing, oversized values)
 *
 * *out is written only on return 1.
 */
int
otlp_http_resp_parse(uint8_t *buf,
	size_t len,
	bool at_eof,
	struct otlp_http_resp *out);

#endif /* OTLP_HTTP_RESPONSE_PARSER_H */
