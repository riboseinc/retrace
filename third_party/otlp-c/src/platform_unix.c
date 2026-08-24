/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * POSIX non-blocking socket primitives. Compiled on Linux, macOS,
 * BSDs. The Win32 equivalent lives in platform_win.c.
 *
 * getaddrinfo is called synchronously (one-shot per request). The
 * connect itself is non-blocking: socket(O_NONBLOCK) + connect(2)
 * returns EINPROGRESS, and the caller drives _finish_connect via
 * poll()/select() on the fd.
 */
/* _POSIX_C_SOURCE for getaddrinfo / strncasecmp under glibc with
 * -std=c11. macOS declares these unconditionally. */
#define _POSIX_C_SOURCE 200809L

#include "platform.h"
#include "internal_util.h"

#include <otlp-c/status.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct otlp_socket
{
	int fd;
	bool eof;
	bool connecting; /* true between connect() and finish_connect() */
};

static void
set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags >= 0)
		(void) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

otlp_status_t
otlp_socket_connect(otlp_socket_t **out, const char *host, uint16_t port)
{
	struct addrinfo hints;
	struct addrinfo *result = NULL;
	struct addrinfo *rp;
	char port_str[8];
	int rc;
	int fd = -1;
	bool connecting = false;
	otlp_socket_t *s;

	if (!out || !host)
		return OTLP_ERR_NULL;

	snprintf(port_str, sizeof(port_str), "%u", (unsigned int) port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;

	rc = getaddrinfo(host, port_str, &hints, &result);
	if (rc != 0)
		return OTLP_ERR_DNS;

	for (rp = result; rp; rp = rp->ai_next)
	{
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd < 0)
			continue;
		set_nonblock(fd);
		if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
		{
			connecting = false;
			break;
		}
		if (errno == EINPROGRESS || errno == EWOULDBLOCK)
		{
			connecting = true;
			break;
		}
		/* Different errno — try next address. */
		close(fd);
		fd = -1;
	}
	freeaddrinfo(result);
	if (fd < 0)
		return OTLP_ERR_CONNECT;

	s = otlp_malloc(sizeof(*s));
	if (!s)
	{
		close(fd);
		return OTLP_ERR_NOMEM;
	}
	s->fd = fd;
	s->eof = false;
	s->connecting = connecting;
	*out = s;
	return OTLP_OK;
}

otlp_status_t
otlp_socket_finish_connect(otlp_socket_t *s)
{
	struct pollfd pfd;
	int rc;
	int err = 0;
	socklen_t len = sizeof(err);

	if (!s || s->fd < 0)
		return OTLP_ERR_NULL;
	if (!s->connecting)
		return OTLP_OK;

	/* Poll for writability first — SO_ERROR can read 0 on macOS
	 * even before the connect completes, which would lead us to
	 * send on an unconnected socket. */
	pfd.fd = s->fd;
	pfd.events = POLLOUT;
	rc = poll(&pfd, 1, 0); /* non-blocking */
	if (rc < 0)
		return OTLP_ERR_CONNECT;
	if (rc == 0)
		return OTLP_ERR_WOULDBLOCK; /* not yet */

	if (getsockopt(s->fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
		return OTLP_ERR_CONNECT;
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
	ssize_t n;

	if (!s || !n_written)
		return OTLP_ERR_NULL;
	if (s->fd < 0)
		return OTLP_ERR_WRITE;
	*n_written = 0;
	if (len == 0)
		return OTLP_OK;

	do
	{
		n = send(s->fd, data, len, MSG_NOSIGNAL);
	} while (n < 0 && errno == EINTR);

	if (n < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return OTLP_ERR_WOULDBLOCK;
		return OTLP_ERR_WRITE;
	}
	*n_written = (size_t) n;
	return OTLP_OK;
}

otlp_status_t
otlp_socket_read(otlp_socket_t *s, uint8_t *buf, size_t cap, size_t *n_read)
{
	ssize_t n;

	if (!s || !n_read)
		return OTLP_ERR_NULL;
	if (s->fd < 0)
		return OTLP_ERR_READ;
	*n_read = 0;
	if (cap == 0)
		return OTLP_OK;

	do
	{
		n = recv(s->fd, buf, cap, 0);
	} while (n < 0 && errno == EINTR);

	if (n < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return OTLP_ERR_WOULDBLOCK;
		return OTLP_ERR_READ;
	}
	if (n == 0)
	{
		s->eof = true;
		return OTLP_OK;
	}
	*n_read = (size_t) n;
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
	if (s->fd >= 0)
		close(s->fd);
	s->fd = -1;
	otlp_free(s);
}

int
otlp_socket_fd(const otlp_socket_t *s)
{
	return s ? s->fd : -1;
}
