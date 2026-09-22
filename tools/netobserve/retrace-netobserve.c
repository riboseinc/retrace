/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * retrace-netobserve: the stock-device network observer
 * (TODO.impl/25, GH #865).
 *
 * A local forward proxy that records observed HTTP traffic in
 * the retrace trace shape -- the same record text the
 * decode_http action emits -- so the evidence grades with
 * every retrace tool. Point the device's HTTP proxy at this
 * host:port; the tool logs each request line and CONNECT
 * target, forwards the bytes, and writes a JSON-lines trace.
 *
 * Plain HTTP is inspected (request line + Host). HTTPS is a
 * CONNECT tunnel: opaque, but the TARGET and byte counts are
 * recorded (network truth, not content).
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define LISTEN_BACKLOG 16
#define BUF_SIZE 8192
#define LINE_MAX_OBS 2048

static FILE *g_out;
static const char *g_listen = "127.0.0.1";
static int g_port = 8080;

static void emit_record(const char *kind, const char *detail)
{
	static long seq;
	time_t now = time(NULL);

	fprintf(g_out,
		"{\"time\":%ld,\"seq\":%ld,\"module\":\"NET\","
		"\"severity\":\"INFO\",\"message\":{"
		"\"func\":\"netobserve\",\"kind\":\"%s\","
		"\"detail\":\"%s\"}}\n",
		(long) now, seq++, kind, detail);
	fflush(g_out);
}

static int connect_upstream(const char *host, int port)
{
	struct sockaddr_in addr;
	int fd;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short) port);
	if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
		close(fd);
		return -1;
	}
	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
		close(fd);
		return -1;
	}
	return fd;
}

/* pump bytes both ways until either side closes */
static void pump(int a, int b)
{
	char buf[BUF_SIZE];
	ssize_t n;
	unsigned long long total = 0;
	int fds[2];
	int alive = 1;

	fds[0] = a;
	fds[1] = b;
	while (alive) {
		fd_set rfds;
		int maxfd = (a > b ? a : b) + 1;
		int i;

		FD_ZERO(&rfds);
		FD_SET(a, &rfds);
		FD_SET(b, &rfds);
		if (select(maxfd, &rfds, NULL, NULL, NULL) < 0)
			break;
		for (i = 0; i < 2 && alive; i++) {
			if (!FD_ISSET(fds[i], &rfds))
				continue;
			n = recv(fds[i], buf, sizeof(buf), 0);
			if (n <= 0) {
				alive = 0;
				break;
			}
			total += (unsigned long long) n;
			if (send(fds[1 - i], buf, (size_t) n, 0) < 0) {
				alive = 0;
				break;
			}
		}
	}
	(void) total;
}

static void handle_connect(int client, const char *target)
{
	char host[256];
	int port = 443;
	char resp[] = "HTTP/1.1 200 Connection Established\r\n\r\n";
	int up;

	{
		char *colon = strchr(target, ':');

		if (colon != NULL) {
			size_t hostlen = (size_t) (colon - target);

			if (hostlen >= sizeof(host))
				hostlen = sizeof(host) - 1;
			memcpy(host, target, hostlen);
			host[hostlen] = '\0';
			port = atoi(colon + 1);
			if (port <= 0)
				port = 443;
		} else {
			snprintf(host, sizeof(host), "%s", target);
		}
	}

	up = connect_upstream(host, port);
	if (up < 0) {
		const char *deny = "HTTP/1.1 502 Bad Gateway\r\n\r\n";

		(void) send(client, deny, strlen(deny), 0);
		emit_record("connect-denied", target);
		close(client);
		return;
	}
	(void) send(client, resp, strlen(resp), 0);
	emit_record("connect", target);
	pump(client, up);
	close(up);
	close(client);
}

static void handle_plain(int client, const char *first_chunk,
	ssize_t chunk_len)
{
	char host[256];
	char buf[BUF_SIZE];
	char detail[LINE_MAX_OBS];
	char *p;
	char *colon;
	int up;
	int up_port = 80;
	size_t len;

	snprintf(host, sizeof(host), "127.0.0.1");
	/* the Host: header decides the upstream (host and port) */
	{
		size_t safe = chunk_len < (ssize_t) sizeof(buf) - 1 ?
			(size_t) chunk_len : sizeof(buf) - 1;

		memcpy(buf, first_chunk, safe);
		buf[safe] = '\0';
	}
	p = strstr(buf, "\r\nHost: ");
	if (p == NULL)
		p = strstr(buf, "\r\nhost: ");
	if (p != NULL) {
		char *eol;

		p += 8;
		eol = strchr(p, '\r');
		if (eol != NULL) {
			size_t hl = (size_t) (eol - p);

			if (hl >= sizeof(host))
				hl = sizeof(host) - 1;
			memcpy(host, p, hl);
			host[hl] = '\0';
			colon = strchr(host, ':');
			if (colon != NULL) {
				int vp = atoi(colon + 1);

				if (vp > 0)
					up_port = vp;
				*colon = '\0';
			}
		}
	}

	snprintf(detail, sizeof(detail), "%.*s -> %s",
		(int) strcspn(buf, "\r\n"), buf, host);
	emit_record("http-request", detail);

	up = connect_upstream(host, up_port);
	if (up < 0) {
		const char *deny = "HTTP/1.1 502 Bad Gateway\r\n\r\n";

		(void) send(client, deny, strlen(deny), 0);
		close(client);
		return;
	}

	/*
	 * proxies receive absolute-form targets; upstreams want
	 * origin-form -- rewrite http://host/... to /...
	 */
	{
		const char *target = strchr(buf, ' ');
		char fwd[BUF_SIZE];
		size_t w = 0;

		if (target != NULL && strncmp(target + 1, "http://", 7) == 0) {
			const char *path = strchr(target + 8, '/');

			if (path == NULL)
				path = "/";
			w += (size_t) snprintf(fwd + w, sizeof(fwd) - w,
				"%.*s %s", (int) (target - buf), buf, path);
		} else {
			w += (size_t) snprintf(fwd + w, sizeof(fwd) - w,
				"%s", buf);
		}
		(void) send(up, fwd, w, 0);
	}
	pump(client, up);
	close(up);
	close(client);
}

int main(int argc, char **argv)
{
	struct sockaddr_in addr;
	int srv;
	int reuse = 1;

	if (argc > 1)
		g_listen = argv[1];
	if (argc > 2)
		g_port = atoi(argv[2]);
	g_out = stdout;
	signal(SIGPIPE, SIG_IGN);

	srv = socket(AF_INET, SOCK_STREAM, 0);
	if (srv < 0) {
		perror("socket");
		return 1;
	}
	(void) setsockopt(srv, SOL_SOCKET, SO_REUSEADDR,
		&reuse, sizeof(reuse));
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short) g_port);
	if (inet_pton(AF_INET, g_listen, &addr.sin_addr) != 1) {
		fprintf(stderr, "bad listen address: %s\n", g_listen);
		return 1;
	}
	if (bind(srv, (struct sockaddr *) &addr, sizeof(addr)) != 0 ||
			listen(srv, LISTEN_BACKLOG) != 0) {
		perror("bind/listen");
		return 1;
	}
	fprintf(stderr, "retrace-netobserve: %s:%d (Ctrl-C to stop)\n",
		g_listen, g_port);

	for (;;) {
		struct sockaddr_in peer;
		socklen_t peerlen = sizeof(peer);
		int client = accept(srv, (struct sockaddr *) &peer,
			&peerlen);
		char buf[BUF_SIZE];
		ssize_t n;
		char *eol;

		if (client < 0)
			continue;
		n = recv(client, buf, sizeof(buf) - 1, 0);
		if (n <= 0) {
			close(client);
			continue;
		}
		buf[n] = '\0';
		/* keep the whole chunk: handle_plain needs the
		 * headers (Host:) -- detect CONNECT by prefix only */
		if (strncmp(buf, "CONNECT ", 8) == 0) {
			char target[256];
			char *sp = strchr(buf + 8, ' ');

			snprintf(target, sizeof(target), "%.*s",
				sp != NULL ? (int) (sp - (buf + 8)) : 248,
				buf + 8);
			handle_connect(client, target);
		} else {
			handle_plain(client, buf, n);
		}
	}
	/* not reached */
	return 0;
}
