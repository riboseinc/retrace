/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Win32 non-blocking socket primitives. Mirrors platform_unix.c.
 *
 * NOTE: written to compile cleanly under MSVC / clang-cl, but the
 * project's primary development and CI machines are POSIX. The
 * Windows CI job in .github/workflows/build.yml exercises this
 * code path; local dev typically does not.
 */
#include "platform.h"
#include "internal_util.h"

#include <otlp-c/status.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

struct otlp_socket
{
	SOCKET s;
	bool eof;
	bool connecting;
};

/* WSAStartup is reference-counted; we lazily init on first use and
 * never call WSACleanup (the process lifetime owns it). */
static otlp_status_t
ensure_wsa(void)
{
	static LONG volatile inited;
	WSADATA wsa;
	LONG prev;

	prev = InterlockedCompareExchange(&inited, 1, 0);
	if (prev != 0)
		return OTLP_OK; /* already initialised */
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return OTLP_ERR_NETWORK;
	return OTLP_OK;
}

static void
set_nonblock(SOCKET s)
{
	u_long mode = 1;

	(void) ioctlsocket(s, FIONBIO, &mode);
}

otlp_status_t
otlp_socket_connect(otlp_socket_t **out, const char *host, uint16_t port)
{
	struct addrinfo hints;
	struct addrinfo *result = NULL;
	struct addrinfo *rp;
	char port_str[8];
	int rc;
	SOCKET s = INVALID_SOCKET;
	bool connecting = false;
	otlp_socket_t *sock;
	otlp_status_t st;

	if (!out || !host)
		return OTLP_ERR_NULL;

	st = ensure_wsa();
	if (st != OTLP_OK)
		return st;

	snprintf(port_str, sizeof(port_str), "%u", (unsigned int) port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	rc = getaddrinfo(host, port_str, &hints, &result);
	if (rc != 0)
		return OTLP_ERR_DNS;

	for (rp = result; rp; rp = rp->ai_next)
	{
		s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (s == INVALID_SOCKET)
			continue;
		set_nonblock(s);
		if (connect(s, rp->ai_addr, (int) rp->ai_addrlen) == 0)
		{
			connecting = false;
			break;
		}
		if (WSAGetLastError() == WSAEWOULDBLOCK)
		{
			connecting = true;
			break;
		}
		closesocket(s);
		s = INVALID_SOCKET;
	}
	freeaddrinfo(result);
	if (s == INVALID_SOCKET)
		return OTLP_ERR_CONNECT;

	sock = otlp_malloc(sizeof(*sock));
	if (!sock)
	{
		closesocket(s);
		return OTLP_ERR_NOMEM;
	}
	sock->s = s;
	sock->eof = false;
	sock->connecting = connecting;
	*out = sock;
	return OTLP_OK;
}

otlp_status_t
otlp_socket_finish_connect(otlp_socket_t *s)
{
	int err = 0;
	int len = sizeof(err);

	if (!s || s->s == INVALID_SOCKET)
		return OTLP_ERR_NULL;
	if (!s->connecting)
		return OTLP_OK;

	if (getsockopt(s->s, SOL_SOCKET, SO_ERROR, (char *) &err, &len) != 0)
		return OTLP_ERR_CONNECT;
	if (err == WSAEINPROGRESS || err == WSAEWOULDBLOCK)
		return OTLP_ERR_WOULDBLOCK;
	if (err != 0)
		return OTLP_ERR_CONNECT;

	s->connecting = false;
	return OTLP_OK;
}

otlp_status_t
otlp_socket_write(otlp_socket_t *s,
	const uint8_t *data,
	size_t len,
	size_t *n_written)
{
	int n;
	int sent;

	if (!s || !n_written)
		return OTLP_ERR_NULL;
	if (s->s == INVALID_SOCKET)
		return OTLP_ERR_WRITE;
	*n_written = 0;
	if (len == 0)
		return OTLP_OK;
	if (len > INT_MAX)
		len = INT_MAX;

	sent = send(s->s, (const char *) data, (int) len, 0);
	if (sent == SOCKET_ERROR)
	{
		int e = WSAGetLastError();
		if (e == WSAEWOULDBLOCK)
			return OTLP_ERR_WOULDBLOCK;
		return OTLP_ERR_WRITE;
	}
	(void) n;
	*n_written = (size_t) sent;
	return OTLP_OK;
}

otlp_status_t
otlp_socket_read(otlp_socket_t *s, uint8_t *buf, size_t cap, size_t *n_read)
{
	int got;

	if (!s || !n_read)
		return OTLP_ERR_NULL;
	if (s->s == INVALID_SOCKET)
		return OTLP_ERR_READ;
	*n_read = 0;
	if (cap == 0)
		return OTLP_OK;
	if (cap > INT_MAX)
		cap = INT_MAX;

	got = recv(s->s, (char *) buf, (int) cap, 0);
	if (got == SOCKET_ERROR)
	{
		int e = WSAGetLastError();
		if (e == WSAEWOULDBLOCK)
			return OTLP_ERR_WOULDBLOCK;
		return OTLP_ERR_READ;
	}
	if (got == 0)
	{
		s->eof = true;
		return OTLP_OK;
	}
	*n_read = (size_t) got;
	return OTLP_OK;
}

int
otlp_socket_eof(const otlp_socket_t *s)
{
	return (s && s->eof) ? 1 : 0;
}

void
otlp_socket_close(otlp_socket_t *s)
{
	if (!s)
		return;
	if (s->s != INVALID_SOCKET)
		closesocket(s->s);
	s->s = INVALID_SOCKET;
	otlp_free(s);
}

int
otlp_socket_fd(const otlp_socket_t *s)
{
	/* Win32 SOCKET is not an fd; we cast through intptr_t for the
	 * benefit of caller code that uses select() (which on Win32
	 * takes SOCKET values in a fd_set, not int fds). */
	return s ? (int) (intptr_t) s->s : -1;
}
