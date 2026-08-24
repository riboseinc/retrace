/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Cross-platform helpers.
 *
 * Clocks: live in platform.c (shared POSIX/Win32 code paths).
 * Sockets: live in platform_unix.c (POSIX) and platform_win.c
 * (Win32), picked by CMake per target platform.
 *
 * NO thread / mutex / condvar abstractions. The library never
 * spawns threads and never takes locks (see plan D3 and the
 * "no library threads" deployment architecture).
 */
#ifndef OTLP_C_PLATFORM_H
#define OTLP_C_PLATFORM_H

#include <otlp-c/status.h>

#include <stddef.h>
#include <stdint.h>

/* ── Clocks (shared, in platform.c) ───────────────────────────── */

otlp_status_t
otlp_platform_now_unix_nano(uint64_t *out);
otlp_status_t
otlp_platform_now_mono_nano(uint64_t *out);

/* Monotonic milliseconds (nano / 1e6, 0 on clock failure). The one
 * shared ms clock — the exporter's tick deadlines and the HTTP
 * client's I/O deadlines both use it (was two private copies). */
uint64_t
otlp_platform_now_mono_ms(void);

/* ── Sockets (always non-blocking; in platform_unix.c / _win.c) ─ */

/* Opaque socket handle. */
typedef struct otlp_socket otlp_socket_t;

/* Initiate a non-blocking connect. getaddrinfo is called
 * synchronously on every connect — results are NOT cached at
 * the library level (the OS resolver usually provides caching
 * via nscd / systemd-resolved / mDNSResponder). With HTTP
 * keep-alive the connection is reused, so DNS lookups are rare
 * in steady state — one per initial connect plus one per
 * reconnect after a connection failure. The connect itself is
 * non-blocking: this returns OK + sets *out to a socket in
 * CONNECTING state; the caller then drives _finish_connect.
 *
 * Returns:
 *   OTLP_OK             — connect initiated (or completed); *out set.
 *   OTLP_ERR_DNS        — host lookup failed.
 *   OTLP_ERR_CONNECT    — could not open a socket.
 *   OTLP_ERR_NOMEM      — allocation failure.
 */
otlp_status_t
otlp_socket_connect(otlp_socket_t **out, const char *host, uint16_t port);

/* Poll completion of a non-blocking connect.
 *
 * Returns:
 *   OTLP_OK               — connect completed; ready to write.
 *   OTLP_ERR_WOULDBLOCK   — still connecting; caller should poll
 *                           the fd for writability and retry.
 *   OTLP_ERR_CONNECT      — connect refused / aborted / timed out.
 */
otlp_status_t
otlp_socket_finish_connect(otlp_socket_t *s);

/* Non-blocking write. *n_written is set to the number of bytes
 * accepted by the kernel buffer (may be less than len).
 *
 * Returns:
 *   OTLP_OK               — *n_written bytes accepted; if < len,
 *                           caller should poll and call again.
 *   OTLP_ERR_WOULDBLOCK   — kernel buffer full; retry after POLLOUT.
 *   OTLP_ERR_WRITE        — connection broken.
 */
otlp_status_t
otlp_socket_write(otlp_socket_t *s,
	const uint8_t *data,
	size_t len,
	size_t *n_written);

/* Non-blocking read. *n_read is set to the number of bytes copied
 * into buf (may be less than cap).
 *
 * Returns:
 *   OTLP_OK               — *n_read bytes available; if 0 and the
 *                           peer closed, this is EOF.
 *   OTLP_ERR_WOULDBLOCK   — nothing to read; retry after POLLIN.
 *   OTLP_ERR_READ         — connection broken.
 */
otlp_status_t
otlp_socket_read(otlp_socket_t *s, uint8_t *buf, size_t cap, size_t *n_read);

/* True if the peer has half-closed (EOF). Valid after a _read that
 * returned OTLP_OK with *n_read == 0. */
int
otlp_socket_eof(const otlp_socket_t *s);

void
otlp_socket_close(otlp_socket_t *s);

/* Raw fd, for the caller's poll()/epoll/etc. -1 after close. */
int
otlp_socket_fd(const otlp_socket_t *s);

#endif
