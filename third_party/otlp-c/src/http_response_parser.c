/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * HTTP/1.1 response wire-format parser — see http_response_parser.h.
 * Extracted verbatim from http_client.c (v0.6.11): status line,
 * line-aligned header scan, Content-Length / Transfer-Encoding /
 * Retry-After / Connection handling, in-place chunked decode, and
 * the RFC 7230 request-smuggling rejections.
 */
/* _POSIX_C_SOURCE for strncasecmp under glibc with -std=c11. */
#define _POSIX_C_SOURCE 200809L

#include "http_response_parser.h"

#include "internal_util.h"

#include <ctype.h>
#include <string.h>
#if defined(_WIN32)
#include <string.h>
#define otlp_strncasecmp _strnicmp
#else
#include <strings.h>
#define otlp_strncasecmp strncasecmp
#endif

/* Retry-After saturation ceiling in SECONDS: 4294967 * 1000 ms
 * still fits uint32_t without wrapping. */
#define OTLP_RETRY_AFTER_MAX_S UINT64_C(4294967)

static int
find_substring(const uint8_t *hay,
	size_t hay_len,
	const char *needle,
	size_t needle_len)
{
	size_t i;

	if (hay_len < needle_len)
		return -1;
	for (i = 0; i <= hay_len - needle_len; i++)
	{
		if (memcmp(hay + i, needle, needle_len) == 0)
			return (int) i;
	}
	return -1;
}

/* Decode a chunked body in place (RFC 7230 §4.1):
 *   chunked-body = *chunk last-chunk trailer-section CRLF
 * The decoded bytes are compacted over the framing (dst <= src at
 * every step, so unread bytes are never clobbered). Returns:
 *   1 — complete; *out_len is the decoded length
 *   0 — need more data
 *  -1 — malformed framing (or a chunk over the response cap)
 */
static int
decode_chunked_in_place(uint8_t *buf, size_t len, size_t *out_len)
{
	size_t src = 0;
	size_t dst = 0;

	for (;;)
	{
		size_t line_end = src;
		size_t size = 0;
		int digits = 0;

		/* chunk-size line: 1*HEXDIG [;ext] CRLF */
		while (line_end < len && buf[line_end] != '\r' &&
			buf[line_end] != ';')
		{
			uint8_t c = buf[line_end];

			if (c >= '0' && c <= '9')
				size = size * 16 + (size_t)(c - '0');
			else if (c >= 'a' && c <= 'f')
				size = size * 16 + (size_t)(c - 'a' + 10);
			else if (c >= 'A' && c <= 'F')
				size = size * 16 + (size_t)(c - 'A' + 10);
			else
				return -1;
			if (size > OTLP_HTTP_RESP_MAX)
				return -1;
			digits++;
			line_end++;
		}
		if (digits == 0 || line_end >= len)
			return 0; /* need more (or empty size line) */
		/* Skip chunk extensions to the CRLF. */
		while (line_end < len && buf[line_end] != '\r')
			line_end++;
		if (line_end + 1 >= len)
			return 0;
		if (buf[line_end + 1] != '\n')
			return -1;

		if (size == 0)
		{
			/* Last chunk. The trailer section starts after
			 * "0\r\n": either the final CRLF (no trailers)
			 * or header lines until an empty line. */
			const uint8_t *t = buf + line_end + 2;
			size_t avail = len - (line_end + 2);

			if (avail < 2)
				return 0;
			if (t[0] == '\r' && t[1] == '\n')
			{
				*out_len = dst;
				return 1;
			}
			{
				int off =
					find_substring(t, avail, "\r\n\r\n", 4);

				if (off < 0)
					return 0; /* trailers incomplete */
				*out_len = dst;
				return 1;
			}
		}
		/* chunk-data CRLF must be fully buffered. */
		if (line_end + 2 + size + 2 > len)
			return 0;
		if (buf[line_end + 2 + size] != '\r' ||
			buf[line_end + 2 + size + 1] != '\n')
			return -1;
		memmove(buf + dst, buf + line_end + 2, size);
		dst += size;
		src = line_end + 2 + size + 2;
	}
}

int
otlp_http_resp_parse(uint8_t *buf,
	size_t len,
	bool at_eof,
	struct otlp_http_resp *out)
{
	int hdr_end_off;
	const char *body_start;
	size_t body_off;
	const char *p;
	long content_length = -1;
	int http_status = 0;
	uint32_t retry_after_ms = 0;
	bool keepalive_eligible;

	hdr_end_off = find_substring(buf, len, "\r\n\r\n", 4);
	if (hdr_end_off < 0)
		return 0;
	body_off = (size_t) hdr_end_off + 4;
	body_start = (const char *) buf + body_off;

	/* Parse status line: "HTTP/1.1 NNN <reason>\r\n". */
	if (len < 12 || memcmp(buf, "HTTP/", 5) != 0)
		return -1;
	/* Find first space, then the 3-digit status. */
	p = (const char *) buf + 5;
	while (p < body_start && *p != ' ')
		p++;
	if (p >= body_start || *p != ' ')
		return -1;
	p++; /* skip space */
	if (p + 3 > body_start || !isdigit((unsigned char) p[0]) ||
		!isdigit((unsigned char) p[1]) ||
		!isdigit((unsigned char) p[2]))
		return -1;
	http_status = (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');

	/* Keep-alive default is version-dependent: HTTP/1.1 defaults
	 * to persistent, HTTP/1.0 (and earlier) to close (RFC 7230
	 * §6.3). Any "Connection: close" wins; "Connection:
	 * keep-alive" upgrades a 1.0 connection. */
	keepalive_eligible = (buf[5] == '1' && buf[6] == '.' && buf[7] >= '1');

	/* Header scan — LINE-ALIGNED (a substring scan over the whole
	 * header block would match inside other headers' values, e.g.
	 * an echoed "X-Note: see Content-Length: 5"). */
	{
		const char *hdr = (const char *) buf;
		const char *hdr_end = (const char *) body_start;
		bool cl_seen = false;
		bool te_seen = false;
		bool te_chunked = false;

		/* Skip the status line. */
		while (hdr < hdr_end && *hdr != '\n')
			hdr++;
		if (hdr >= hdr_end)
			return -1;
		hdr++;

		while (hdr < hdr_end)
		{
			const char *line = hdr;
			const char *eol = hdr;

			while (eol < hdr_end && *eol != '\r')
				eol++;
			if (eol >= hdr_end)
				return -1;
			hdr = eol + 2; /* skip CRLF */

			if (otlp_strncasecmp(line, "Content-Length:", 15) == 0)
			{
				const char *v = line + 15;
				long val = 0;

				while (v < eol && *v == ' ')
					v++;
				while (v < eol && isdigit((unsigned char) *v))
				{
					val = val * 10 + (*v - '0');
					if (val > OTLP_HTTP_RESP_MAX)
						return -1;
					v++;
				}
				/* Duplicate Content-Length with a DIFFERENT
				 * value is a request-smuggling vector
				 * (RFC 7230 §3.3.2); reject. Identical
				 * duplicates are legal and collapse. */
				if (cl_seen && val != content_length)
					return -1;
				content_length = val;
				cl_seen = true;
			}
			else if (otlp_strncasecmp(
					 line, "Transfer-Encoding:", 18) == 0)
			{
				const char *v = line + 18;

				te_seen = true;
				while (v < eol && *v == ' ')
					v++;
				/* Only bare "chunked" is decodable; any other
				 * coding (gzip, identity, ...) is rejected —
				 * we cannot frame or decode it. */
				if ((size_t)(eol - v) == 7 &&
					otlp_strncasecmp(v, "chunked", 7) == 0)
					te_chunked = true;
			}
			else if (otlp_strncasecmp(line, "Retry-After:", 12) ==
				0)
			{
				const char *v = line + 12;
				uint64_t sec = 0;
				int digits = 0;

				while (v < eol && *v == ' ')
					v++;
				/* Cap at 10 digits: any value that large is
				 * ≥ 1e9 s (31 years) — saturate below, and
				 * the bounded digit count keeps `sec` far
				 * from uint64 overflow (CWE-190). */
				while (v < eol && isdigit((unsigned char) *v) &&
					digits < 10)
				{
					sec = sec * 10 + (uint64_t)(*v - '0');
					digits++;
					v++;
				}
				if (sec > OTLP_RETRY_AFTER_MAX_S)
					sec = OTLP_RETRY_AFTER_MAX_S;
				/* delta-seconds form only: an HTTP-date
				 * value (first char not a digit) leaves
				 * sec at 0 — treated as absent. Duplicates:
				 * last one wins (not a framing header, so
				 * no smuggling ambiguity). */
				retry_after_ms = (uint32_t)(sec * 1000);
			}
			else if (otlp_strncasecmp(line, "Connection:", 11) == 0)
			{
				const char *v = line + 11;

				while (v < eol && *v == ' ')
					v++;
				if (otlp_strncasecmp(v, "close", 5) == 0)
					keepalive_eligible = false;
				else if ((size_t)(eol - v) >= 10 &&
					otlp_strncasecmp(v, "keep-alive", 10) ==
						0)
					keepalive_eligible = true;
			}
		}

		/* Content-Length AND Transfer-Encoding together is the
		 * classic smuggling vector (RFC 7230 §3.3.3 rule 3):
		 * reject rather than guess. */
		if (te_seen)
		{
			if (cl_seen || !te_chunked)
				return -1;
			{
				size_t decoded = 0;
				int rc = decode_chunked_in_place(buf + body_off,
					len - body_off,
					&decoded);

				if (rc == 0)
					return 0; /* need more chunks */
				if (rc < 0)
					return -1;
				/* Chunked framing is self-delimiting: the
				 * connection stays reusable. */
				out->http_status = http_status;
				out->retry_after_ms = retry_after_ms;
				out->keepalive_eligible = keepalive_eligible;
				out->body = buf + body_off;
				out->body_len = decoded;
				return 1;
			}
		}
	}

	if (content_length >= 0)
	{
		/* Need exactly content_length bytes after \r\n\r\n. */
		if (len - body_off < (size_t) content_length)
			return 0;
		out->body = (const uint8_t *) body_start;
		out->body_len = (size_t) content_length;
	}
	else
	{
		/* No Content-Length: body extends until EOF. Without EOF,
		 * more body bytes might still arrive — do not declare
		 * complete. RFC 7230 §3.3.3 (7). */
		if (!at_eof)
			return 0;
		out->body = (const uint8_t *) body_start;
		out->body_len = len - body_off;
		/* Server sent no Content-Length → framing is ambiguous.
		 * Disable keep-alive; the connection must close. */
		keepalive_eligible = false;
	}
	out->http_status = http_status;
	out->retry_after_ms = retry_after_ms;
	out->keepalive_eligible = keepalive_eligible;
	return 1;
}
