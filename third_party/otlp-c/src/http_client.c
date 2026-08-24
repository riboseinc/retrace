/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * HTTP/1.1 POST state machine. See src/http_client.h for the API.
 *
 * Wire format produced (POST, plain HTTP, Connection: keep-alive):
 *
 *   POST <path> HTTP/1.1\r\n
 *   Host: <host>\r\n
 *   User-Agent: <ua>\r\n
 *   Content-Type: application/x-protobuf\r\n
 *   Content-Length: <n>\r\n
 *   Connection: keep-alive\r\n
 *   \r\n
 *   <body>
 *
 * Response parsing: minimal. We scan for "HTTP/1.1 NNN" status line
 * and a Content-Length header; the body is everything after \r\n\r\n
 * up to Content-Length bytes. If the response includes
 * `Connection: close`, the socket is closed at _free; otherwise
 * (HTTP/1.1 default) the socket is reusable via _detach_socket.
 */
#include "http_client.h"
#include "http_response_parser.h"
#include "internal_util.h"
#include "platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── URL parser ───────────────────────────────────────────────── */

static int
parse_uint16(const char *s, size_t len, uint16_t *out)
{
	uint32_t v = 0;
	size_t i;

	for (i = 0; i < len; i++)
	{
		if (s[i] < '0' || s[i] > '9')
			return -1;
		v = v * 10u + (uint32_t)(s[i] - '0');
		if (v > 65535u)
			return -1;
	}
	if (i == 0)
		return -1;
	*out = (uint16_t) v;
	return 0;
}

otlp_status_t
otlp_http_parse_url(const char *url, struct otlp_http_url *out)
{
	const char *scheme_end;
	const char *host_start;
	const char *host_end;
	const char *p;
	size_t host_len;
	size_t path_len;

	if (!url || !out)
		return OTLP_ERR_NULL;

	/* Reject CR/LF anywhere in the URL. Without this, a caller-
	 * controlled URL containing "\r\n" could inject arbitrary HTTP
	 * headers into the request line / Host header (HTTP request
	 * splitting, CWE-93). */
	for (p = url; *p != '\0'; p++)
		if (*p == '\r' || *p == '\n')
			return OTLP_ERR_INVALID_ARGUMENT;

	/* Accept only http://. */
	if (strncmp(url, "http://", 7) != 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	scheme_end = url + 7;
	if (*scheme_end == '\0' || *scheme_end == '/')
		return OTLP_ERR_INVALID_ARGUMENT;

	/* Find end of host (':', '/', or '\0'). */
	host_start = scheme_end;
	host_end = host_start;
	while (*host_end != '\0' && *host_end != ':' && *host_end != '/')
		host_end++;
	host_len = (size_t)(host_end - host_start);
	if (host_len == 0 || host_len >= OTLP_HTTP_HOST_MAX)
		return OTLP_ERR_INVALID_ARGUMENT;
	memcpy(out->host, host_start, host_len);
	out->host[host_len] = '\0';

	/* Optional port. */
	if (*host_end == ':')
	{
		const char *port_start = host_end + 1;
		const char *port_end = port_start;

		while (*port_end != '\0' && *port_end != '/')
			port_end++;
		if (parse_uint16(port_start,
			    (size_t)(port_end - port_start),
			    &out->port) != 0)
			return OTLP_ERR_INVALID_ARGUMENT;
		host_end = port_end;
	}
	else
	{
		out->port = 80;
	}

	/* Path: rest of the URL (may be empty → "/"). */
	p = host_end;
	if (*p == '\0')
	{
		out->path[0] = '/';
		out->path[1] = '\0';
	}
	else
	{
		path_len = strlen(p);
		if (path_len >= OTLP_HTTP_PATH_MAX)
			return OTLP_ERR_INVALID_ARGUMENT;
		memcpy(out->path, p, path_len + 1);
	}
	return OTLP_OK;
}

/* ── Request state ────────────────────────────────────────────── */


struct otlp_http_request
{
	otlp_http_req_state_t state;
	struct otlp_socket *sock;

	/* Encoded request bytes (header + body). */
	uint8_t *req_buf;
	size_t req_len;
	size_t req_sent;

	/* Response accumulation. */
	uint8_t *resp_buf;
	size_t resp_cap;
	size_t resp_len;

	/* Parsed response. */
	int http_status;
	uint32_t retry_after_ms; /* Retry-After header, ms (0 = absent) */
	const uint8_t *body_ptr; /* into resp_buf */
	size_t body_len;
	bool keepalive_eligible; /* set by response parser */

	/* Deadline enforcement (0 = no timeout / infinite). Stored as
	 * the original duration; the step functions compute "has the
	 * deadline elapsed?" by comparing now to the request start time
	 * (for connect) or the last successful recv (for read). */
	uint32_t connect_timeout_ms;
	uint32_t read_timeout_ms;
	uint64_t start_ms; /* monotonic ms at _start time */
	uint64_t last_io_ms; /* monotonic ms at most recent I/O
			      * progress (connect done, partial send,
			      * bytes received) */
};

static otlp_status_t
build_request(struct otlp_http_request *r,
	const struct otlp_http_url *url,
	const char *user_agent,
	const uint8_t *body,
	size_t body_len)
{
	char head[1024];
	int n;
	size_t total;
	const char *ua = user_agent ? user_agent : "otlp-c";
	const char *p;

	/* Reject CR/LF in any field that ends up in a header line.
	 * Without this, a caller-controlled user_agent containing
	 * "\r\n" could inject arbitrary HTTP headers (CWE-93). The
	 * URL is validated at parse time, but user_agent is caller-
	 * supplied and never sanitized before this point. */
	for (p = ua; *p != '\0'; p++)
		if (*p == '\r' || *p == '\n')
			return OTLP_ERR_INVALID_ARGUMENT;

	/* Content-Length is always present; we don't chunk. */
	n = snprintf(head,
		sizeof(head),
		"POST %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"User-Agent: %s\r\n"
		"Content-Type: application/x-protobuf\r\n"
		"Content-Length: %zu\r\n"
		"Connection: keep-alive\r\n"
		"\r\n",
		url->path,
		url->host,
		ua,
		body_len);
	if (n < 0 || (size_t) n >= sizeof(head))
		return OTLP_ERR_OVERFLOW;

	total = (size_t) n + body_len;
	if (total < body_len) /* overflow check */
		return OTLP_ERR_OVERFLOW;

	r->req_buf = otlp_malloc(total);
	if (!r->req_buf)
		return OTLP_ERR_NOMEM;
	memcpy(r->req_buf, head, (size_t) n);
	if (body_len > 0)
		memcpy(r->req_buf + (size_t) n, body, body_len);
	r->req_len = total;
	r->req_sent = 0;
	return OTLP_OK;
}

otlp_status_t
otlp_http_request_start_with_socket(otlp_http_request_t **out,
	const struct otlp_http_url *url,
	const char *user_agent,
	const uint8_t *body,
	size_t body_len,
	uint32_t connect_timeout_ms,
	uint32_t read_timeout_ms,
	struct otlp_socket *donated_socket)
{
	struct otlp_http_request *r;
	otlp_status_t st;

	if (!out || !url)
		return OTLP_ERR_NULL;
	if (body_len > 0 && !body)
		return OTLP_ERR_NULL;
	if (!donated_socket)
		return OTLP_ERR_NULL;

	r = otlp_calloc(1, sizeof(*r));
	if (!r)
		return OTLP_ERR_NOMEM;

	/* Donated socket: skip CONNECTING, go straight to SENDING. */
	r->state = OTLP_HTTP_REQ_SENDING;
	r->sock = donated_socket;

	/* Store timeout durations + start time for deadline checks in
	 * step. 0 means no timeout (infinite). */
	r->connect_timeout_ms = connect_timeout_ms;
	r->read_timeout_ms = read_timeout_ms;
	r->start_ms = otlp_platform_now_mono_ms();
	r->last_io_ms = r->start_ms;

	st = build_request(r, url, user_agent, body, body_len);
	if (st != OTLP_OK)
		goto fail;

	*out = r;
	return OTLP_OK;

fail:
	if (r->sock)
		otlp_socket_close(r->sock);
	otlp_free(r->req_buf);
	otlp_free(r);
	return st;
}

otlp_status_t
otlp_http_request_start(otlp_http_request_t **out,
	const struct otlp_http_url *url,
	const char *user_agent,
	const uint8_t *body,
	size_t body_len,
	uint32_t connect_timeout_ms,
	uint32_t read_timeout_ms)
{
	struct otlp_http_request *r;
	otlp_status_t st;

	if (!out || !url)
		return OTLP_ERR_NULL;
	if (body_len > 0 && !body)
		return OTLP_ERR_NULL;

	r = otlp_calloc(1, sizeof(*r));
	if (!r)
		return OTLP_ERR_NOMEM;
	r->state = OTLP_HTTP_REQ_CONNECTING;

	/* Store timeout durations + start time for deadline checks in
	 * step. 0 means no timeout (infinite). */
	r->connect_timeout_ms = connect_timeout_ms;
	r->read_timeout_ms = read_timeout_ms;

	st = build_request(r, url, user_agent, body, body_len);
	if (st != OTLP_OK)
		goto fail;

	st = otlp_socket_connect(&r->sock, url->host, url->port);
	if (st != OTLP_OK)
		goto fail;

	/* Start the deadline clock AFTER getaddrinfo + connect initiation.
	 * The blocking DNS lookup can take seconds; measuring the connect
	 * timeout from before it would make the deadline fire prematurely. */
	r->start_ms = otlp_platform_now_mono_ms();
	r->last_io_ms = r->start_ms;

	/* If connect() completed synchronously (rare for non-blocking
	 * on the first call), advance state to SENDING. */
	st = otlp_socket_finish_connect(r->sock);
	if (st == OTLP_OK)
		r->state = OTLP_HTTP_REQ_SENDING;
	else if (st == OTLP_ERR_WOULDBLOCK)
		(void) st; /* leave in CONNECTING */
	else
		goto fail;

	*out = r;
	return OTLP_OK;

fail:
	if (r->sock)
		otlp_socket_close(r->sock);
	otlp_free(r->req_buf);
	otlp_free(r);
	return st;
}

struct otlp_socket *
otlp_http_request_detach_socket(otlp_http_request_t *r)
{
	struct otlp_socket *sock;

	if (!r || r->state != OTLP_HTTP_REQ_DONE || !r->keepalive_eligible)
		return NULL;
	sock = r->sock;
	r->sock = NULL;
	return sock;
}

/* ── Response parser ──────────────────────────────────────────── */

/* Response wire-format parsing lives in http_response_parser.c
 * (pure bytes -> verdict). This adapter feeds the accumulated
 * buffer and commits the parsed fields to the request. */
static int
try_parse_response(struct otlp_http_request *r, bool at_eof)
{
	struct otlp_http_resp resp;
	int rc = otlp_http_resp_parse(r->resp_buf, r->resp_len, at_eof, &resp);

	if (rc == 1)
	{
		r->http_status = resp.http_status;
		r->retry_after_ms = resp.retry_after_ms;
		r->keepalive_eligible = resp.keepalive_eligible;
		r->body_ptr = resp.body;
		r->body_len = resp.body_len;
	}
	return rc;
}

/* ── Step ─────────────────────────────────────────────────────── */

static otlp_status_t
step_connecting(struct otlp_http_request *r)
{
	otlp_status_t st = otlp_socket_finish_connect(r->sock);

	if (st == OTLP_OK)
	{
		r->state = OTLP_HTTP_REQ_SENDING;
		r->last_io_ms = otlp_platform_now_mono_ms();
		return OTLP_OK;
	}
	/* Deadline check: if the caller set a connect timeout and it
	 * has elapsed since start, fail the request rather than waiting
	 * forever for an unreachable collector. */
	if (st == OTLP_ERR_WOULDBLOCK && r->connect_timeout_ms != 0 &&
		otlp_platform_now_mono_ms() - r->start_ms >=
			r->connect_timeout_ms)
	{
		r->state = OTLP_HTTP_REQ_FAILED;
		return OTLP_ERR_TIMEOUT;
	}
	return st; /* WOULDBLOCK or error */
}

static otlp_status_t
step_sending(struct otlp_http_request *r)
{
	size_t n_written;
	otlp_status_t st;

	st = otlp_socket_write(r->sock,
		r->req_buf + r->req_sent,
		r->req_len - r->req_sent,
		&n_written);
	if (st == OTLP_ERR_WOULDBLOCK)
	{
		/* A server that accepts but never reads (send-side
		 * slowloris) would block SENDING forever: the same
		 * read_timeout_ms inactivity deadline applies. */
		if (r->read_timeout_ms != 0 &&
			otlp_platform_now_mono_ms() - r->last_io_ms >=
				r->read_timeout_ms)
		{
			r->state = OTLP_HTTP_REQ_FAILED;
			return OTLP_ERR_TIMEOUT;
		}
		return OTLP_ERR_WOULDBLOCK;
	}
	if (st != OTLP_OK)
		return st;
	if (n_written > 0)
		r->last_io_ms = otlp_platform_now_mono_ms();
	r->req_sent += n_written;
	if (r->req_sent == r->req_len)
	{
		r->state = OTLP_HTTP_REQ_READING;
		/* Allocate response buffer now. */
		r->resp_cap = 4096;
		r->resp_buf = otlp_malloc(r->resp_cap);
		if (!r->resp_buf)
			return OTLP_ERR_NOMEM;
		r->resp_len = 0;
	}
	return OTLP_OK;
}

static otlp_status_t
step_reading(struct otlp_http_request *r)
{
	uint8_t small[4096];
	size_t n_read;
	otlp_status_t st;
	int parsed;

	st = otlp_socket_read(r->sock, small, sizeof(small), &n_read);
	if (st == OTLP_ERR_WOULDBLOCK)
	{
		/* Deadline check: if the caller set a read timeout and it
		 * has elapsed since the last successful recv (or start),
		 * fail the request. */
		if (r->read_timeout_ms != 0 &&
			otlp_platform_now_mono_ms() - r->last_io_ms >=
				r->read_timeout_ms)
		{
			r->state = OTLP_HTTP_REQ_FAILED;
			return OTLP_ERR_TIMEOUT;
		}
		return OTLP_ERR_WOULDBLOCK;
	}
	if (st != OTLP_OK)
		return st;

	if (n_read > 0)
	{
		/* Grow if needed. */
		if (r->resp_len + n_read > r->resp_cap)
		{
			size_t new_cap = r->resp_cap;
			uint8_t *p;

			while (new_cap < r->resp_len + n_read)
			{
				if (new_cap > OTLP_HTTP_RESP_MAX)
					return OTLP_ERR_OVERFLOW;
				new_cap *= 2;
			}
			p = otlp_realloc(r->resp_buf, new_cap);
			if (!p)
				return OTLP_ERR_NOMEM;
			r->resp_buf = p;
			r->resp_cap = new_cap;
		}
		memcpy(r->resp_buf + r->resp_len, small, n_read);
		r->resp_len += n_read;
		/* Reset the inter-recv timer: a slow-but-steady stream
		 * should not time out as long as bytes keep arriving. */
		r->last_io_ms = otlp_platform_now_mono_ms();
	}

	parsed = try_parse_response(r, false);
	if (parsed < 0)
		return OTLP_ERR_INVALID_RESPONSE;
	if (parsed == 1)
	{
		r->state = OTLP_HTTP_REQ_DONE;
		return OTLP_OK;
	}
	/* parsed == 0: need more data, OR (no Content-Length) need EOF. */
	if (otlp_socket_eof(r->sock))
	{
		/* Peer closed. Final parse with at_eof=true: for the
		 * no-Content-Length case, the body is whatever was buffered
		 * before EOF. For the Content-Length case, this re-parse
		 * still requires the body to be fully received. */
		if (try_parse_response(r, true) == 1)
		{
			r->state = OTLP_HTTP_REQ_DONE;
			return OTLP_OK;
		}
		return OTLP_ERR_INVALID_RESPONSE;
	}
	return OTLP_OK; /* try _step again later */
}

otlp_status_t
otlp_http_request_step(otlp_http_request_t *r)
{
	otlp_status_t st;

	if (!r)
		return OTLP_ERR_NULL;
	switch (r->state)
	{
		case OTLP_HTTP_REQ_CONNECTING:
			st = step_connecting(r);
			break;
		case OTLP_HTTP_REQ_SENDING:
			st = step_sending(r);
			break;
		case OTLP_HTTP_REQ_READING:
			st = step_reading(r);
			break;
		default:
			return OTLP_OK; /* terminal: no-op */
	}
	if (st != OTLP_OK && st != OTLP_ERR_WOULDBLOCK)
		r->state = OTLP_HTTP_REQ_FAILED;
	return st;
}

otlp_http_req_state_t
otlp_http_request_state(const otlp_http_request_t *r)
{
	return r ? r->state : OTLP_HTTP_REQ_FAILED;
}

int
otlp_http_request_fd(const otlp_http_request_t *r)
{
	return r && r->sock ? otlp_socket_fd(r->sock) : -1;
}

int
otlp_http_request_events(const otlp_http_request_t *r)
{
	if (!r)
		return 0;
	switch (r->state)
	{
		case OTLP_HTTP_REQ_CONNECTING:
		case OTLP_HTTP_REQ_SENDING:
			return 2; /* POLLOUT */
		case OTLP_HTTP_REQ_READING:
			return 1; /* POLLIN */
		default:
			return 0;
	}
}

int
otlp_http_request_http_status(const otlp_http_request_t *r)
{
	return r ? r->http_status : 0;
}

uint32_t
otlp_http_request_retry_after_ms(const otlp_http_request_t *r)
{
	return r ? r->retry_after_ms : 0;
}

const uint8_t *
otlp_http_request_body(const otlp_http_request_t *r, size_t *len_out)
{
	if (len_out)
		*len_out = r ? r->body_len : 0;
	return r ? r->body_ptr : NULL;
}

void
otlp_http_request_free(otlp_http_request_t *r)
{
	if (!r)
		return;
	if (r->sock)
		otlp_socket_close(r->sock);
	otlp_free(r->req_buf);
	otlp_free(r->resp_buf);
	otlp_free(r);
}
