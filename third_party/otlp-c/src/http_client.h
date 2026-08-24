/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Non-blocking HTTP/1.1 POST client. Internal-only API.
 *
 * A POST is a state machine: otlp_http_request_start initiates a
 * non-blocking connect, otlp_http_request_step advances the
 * machine (connect → write request → read response → done), and
 * the caller polls the fd between steps. No internal threads, no
 * locks. The exporter's tick() drives each in-flight request.
 *
 * Only http:// (plain) is supported. TLS termination is the
 * otelcol sidecar's job — see docs/deployment.md.
 */
#ifndef OTLP_C_HTTP_CLIENT_H
#define OTLP_C_HTTP_CLIENT_H

#include <otlp-c/status.h>

#include "platform.h"

#include <stddef.h>
#include <stdint.h>

#define OTLP_HTTP_HOST_MAX 256
#define OTLP_HTTP_PATH_MAX 256

struct otlp_http_url
{
	char host[OTLP_HTTP_HOST_MAX];
	uint16_t port;
	char path[OTLP_HTTP_PATH_MAX];
};

/* Parse "http://host[:port]/path" into url. Accepts only http://.
 * Rejects https://, missing host, port out of range, etc.
 *
 * Default port: 80. Default path: "/". */
otlp_status_t
otlp_http_parse_url(const char *url, struct otlp_http_url *out);

/* ── Request state machine ────────────────────────────────────── */

typedef enum
{
	OTLP_HTTP_REQ_CONNECTING,
	OTLP_HTTP_REQ_SENDING,
	OTLP_HTTP_REQ_READING,
	OTLP_HTTP_REQ_DONE, /* terminal success */
	OTLP_HTTP_REQ_FAILED /* terminal failure */
} otlp_http_req_state_t;

typedef struct otlp_http_request otlp_http_request_t;

/* Start a POST. Copies body. Initiates non-blocking connect.
 *
 * connect_timeout_ms: deadline for the CONNECTING phase.
 * read_timeout_ms: INACTIVITY deadline covering the SENDING and
 * READING phases — it resets on every send/recv progress, so a
 * slow-but-steady stream never trips it, but a stalled peer does
 * (including a server that accepts the connection yet never reads
 * the request). There is no total-duration cap; callers needing
 * one use tick's max_wait_ms / flush_timeout_ms.
 * 0 means no timeout (infinite).
 *
 * Returns:
 *   OTLP_OK             — request started; in CONNECTING state.
 *   OTLP_ERR_NOMEM      — allocation failure.
 *   OTLP_ERR_NULL       — out / url / body (if body_len > 0) is NULL.
 *   OTLP_ERR_DNS        — host lookup failed.
 *   OTLP_ERR_CONNECT    — could not initiate connect.
 *   OTLP_ERR_INVALID_ARGUMENT — URL parse failure. */
otlp_status_t
otlp_http_request_start(otlp_http_request_t **out,
	const struct otlp_http_url *url,
	const char *user_agent,
	const uint8_t *body,
	size_t body_len,
	uint32_t connect_timeout_ms,
	uint32_t read_timeout_ms);

/* Same as _start, but reuses a previously-connected socket instead
 * of opening a new TCP connection. The request takes ownership of
 * `donated_socket`; it will be closed by _free unless retrieved via
 * _detach_socket.
 *
 * Caller is responsible for ensuring the socket is connected to
 * url->host:url->port. Mismatched host:port is undefined behavior.
 *
 * The request enters SENDING state directly (no CONNECTING phase).
 * connect_timeout_ms is informational (the socket is already
 * connected) but accepted for API symmetry.
 *
 * Timeout semantics same as _start. */
otlp_status_t
otlp_http_request_start_with_socket(otlp_http_request_t **out,
	const struct otlp_http_url *url,
	const char *user_agent,
	const uint8_t *body,
	size_t body_len,
	uint32_t connect_timeout_ms,
	uint32_t read_timeout_ms,
	otlp_socket_t *donated_socket);

/* Detach the underlying socket from a completed (DONE state) request.
 * The request no longer owns the socket; the caller must close it
 * (or donate it to another request via _start_with_socket).
 *
 * Returns NULL if the request is not in DONE state, or if the
 * response indicated `Connection: close`. In those cases the socket
 * has already been closed by _free. */
otlp_socket_t *
otlp_http_request_detach_socket(otlp_http_request_t *req);

/* Advance the state machine by one non-blocking iteration.
 * Returns:
 *   OTLP_OK               — useful work was done; check _state().
 *   OTLP_ERR_WOULDBLOCK   — caller should poll fd and call again.
 *   Other OTLP_ERR_*      — terminal; state set to FAILED. */
otlp_status_t
otlp_http_request_step(otlp_http_request_t *req);

otlp_http_req_state_t
otlp_http_request_state(const otlp_http_request_t *req);

/* Raw fd for caller's poll()/epoll/etc. */
int
otlp_http_request_fd(const otlp_http_request_t *req);

/* POLLIN=1, POLLOUT=2 bits — what to wait for, given the state.
 * Returns 0 if the request is terminal. */
int
otlp_http_request_events(const otlp_http_request_t *req);

/* Valid in DONE state. HTTP status code (e.g. 200, 404, 500).
 * 0 if response was malformed. */
int
otlp_http_request_http_status(const otlp_http_request_t *req);

/* Valid in DONE state. The response's Retry-After header converted
 * to milliseconds (RFC 7231 §7.1.3). Only the delta-seconds form is
 * understood; an HTTP-date value or a missing header returns 0.
 * Duplicate headers: the last one wins. Absurdly large values
 * saturate at 4294967000 ms rather than wrapping. */
uint32_t
otlp_http_request_retry_after_ms(const otlp_http_request_t *req);

/* Valid in DONE state. Pointer into the request's internal buffer;
 * freed by _free. */
const uint8_t *
otlp_http_request_body(const otlp_http_request_t *req, size_t *len_out);

void
otlp_http_request_free(otlp_http_request_t *req);

#endif
