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
 * sockaddr_inspect implementation. See sockaddr_inspect.h for the
 * public contract.
 *
 * All libc usage goes through retrace_real_impls. We never call
 * strchr/strncpy/strtol directly because those are intercepted by
 * prototypes/string.c -- using them would recurse back through the
 * engine. Manual equivalents below.
 */

#ifdef _WIN32
#include "posix_compat.h" /* windows.h first + macro hygiene */
#include <stddef.h>

/*
 * Winsock has no sa_family_t and no unix-domain sockets; the
 * inspector's AF_UNIX branch is POSIX-only at runtime, so the
 * types only need to exist for compilation on Windows.
 */
typedef unsigned short sa_family_t_win;
#define sa_family_t sa_family_t_win
#ifndef AF_UNIX
struct sockaddr_un_win {
	unsigned short sun_family;
	char sun_path[108];
};
#define sockaddr_un sockaddr_un_win
#define AF_UNIX 1
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif
#ifndef _WIN32
#include <sys/types.h>
#include <netinet/in.h>
#endif
#ifndef _WIN32
#include <sys/un.h>
#endif
#ifndef _WIN32
#include <arpa/inet.h>
#endif

#include "sockaddr_inspect.h"
#include "real_impls.h"
#include "logger.h"

#define MIN_HEADER_LEN  (sizeof(((struct sockaddr *)0)->sa_family))

static char find_char(const char *s, char c)
{
	for (; *s != '\0'; s++) {
		if (*s == c)
			return 1;
	}
	return 0;
}

static const char *find_byte(const char *s, char c)
{
	for (; *s != '\0'; s++) {
		if (*s == c)
			return s;
	}
	return NULL;
}

static void copy_bounded(const char *src, char *dst, size_t cap)
{
	size_t i;

	if (cap == 0)
		return;

	for (i = 0; i + 1 < cap; i++) {
		if (src[i] == '\0')
			break;
		dst[i] = src[i];
	}
	dst[i] = '\0';
}

/* Parse a non-negative decimal integer 0..65535. Returns 0 on
 * success and writes the value; -1 on overflow or empty input.
 */
static int parse_port(const char *s, int *out)
{
	long v = 0;

	if (s[0] < '0' || s[0] > '9')
		return -1;

	while (s[0] >= '0' && s[0] <= '9') {
		v = v * 10 + (s[0] - '0');
		if (v > 65535)
			return -1;
		s++;
	}

	if (s[0] != '\0')
		return -1;

	*out = (int)v;
	return 0;
}

static void format_ipv4(const struct in_addr *a, char *buf, size_t buflen)
{
	const unsigned char *p = (const unsigned char *)a;

	if (buflen < 16) {
		if (buflen > 0)
			buf[0] = '\0';
		return;
	}

	retrace_real_impls.real_snprintf(buf, buflen, "%u.%u.%u.%u",
		p[0], p[1], p[2], p[3]);
}

static void format_ipv6(const struct in6_addr *a, char *buf, size_t buflen)
{
	const uint16_t *w = (const uint16_t *)a->s6_addr;
	size_t i;
	size_t pos = 0;

	if (buflen < 40) {
		if (buflen > 0)
			buf[0] = '\0';
		return;
	}

	for (i = 0; i < 8; i++) {
		uint16_t v = ntohs(w[i]);
		int n;

		n = retrace_real_impls.real_snprintf(buf + pos,
			buflen - pos, (i == 0) ? "%x" : ":%x", v);
		if (n < 0 || (size_t)n >= buflen - pos)
			break;
		pos += (size_t)n;
	}
}

int retrace_sockaddr_inspect(const void *sa, size_t addrlen,
			      struct retrace_sockaddr_info *out)
{
	const struct sockaddr *s;
	sa_family_t family;

	if (sa == NULL || out == NULL)
		return -1;

	retrace_real_impls.memset(out, 0, sizeof(*out));

	if (addrlen != 0 && addrlen < MIN_HEADER_LEN) {
		log_err("sockaddr_inspect: addrlen %zu too small", addrlen);
		return -1;
	}

	s = (const struct sockaddr *)sa;
	family = s->sa_family;
	out->family = (int)family;

	switch (family) {
	case AF_INET: {
		const struct sockaddr_in *sin;

		if (addrlen != 0 && addrlen < sizeof(*sin))
			return -1;

		sin = (const struct sockaddr_in *)sa;
		out->group = RETRACE_SAF_INET;
		out->expected_len = sizeof(*sin);
		out->port = ntohs(sin->sin_port);
		format_ipv4(&sin->sin_addr, out->ip, sizeof(out->ip));
		return 0;
	}
	case AF_INET6: {
		const struct sockaddr_in6 *sin6;

		if (addrlen != 0 && addrlen < sizeof(*sin6))
			return -1;

		sin6 = (const struct sockaddr_in6 *)sa;
		out->group = RETRACE_SAF_INET6;
		out->expected_len = sizeof(*sin6);
		out->port = ntohs(sin6->sin6_port);
		format_ipv6(&sin6->sin6_addr, out->ip, sizeof(out->ip));
		return 0;
	}
	case AF_UNIX: {
		const struct sockaddr_un *sun;
		size_t pathlen;
		size_t max_path = sizeof(sun->sun_path);

		if (addrlen != 0 && addrlen < sizeof(sa_family_t))
			return -1;

		sun = (const struct sockaddr_un *)sa;
		out->group = RETRACE_SAF_UNIX;
		out->expected_len = sizeof(*sun);

		if (addrlen != 0 && addrlen - sizeof(sa_family_t) < max_path)
			pathlen = addrlen - sizeof(sa_family_t);
		else
			pathlen = max_path;

		/* Copy up to pathlen bytes, NUL-terminate. */
		if (pathlen > sizeof(out->path) - 1)
			pathlen = sizeof(out->path) - 1;

		retrace_real_impls.memcpy(out->path, sun->sun_path, pathlen);
		out->path[pathlen] = '\0';

		/* If the source had a NUL before pathlen, truncate. */
		{
			size_t nulpos = 0;

			while (nulpos < pathlen &&
			       out->path[nulpos] != '\0')
				nulpos++;
			out->path[nulpos] = '\0';
		}
		return 0;
	}
	default:
		out->group = RETRACE_SAF_UNKNOWN;
		return 0;
	}
}

/* Parse a "[literal]:port" or "host:port" spec. Returns 0 on
 * success, -1 on malformed. On success:
 *   host_buf   - host part (no brackets), NUL-terminated
 *   host_len   - length of host part (excluding NUL)
 *   has_port   - 1 if a port was given, 0 if wildcard or absent
 *   port       - 0 if has_port==0, else the parsed port
 */
static int parse_host_port(const char *spec,
			   char *host_buf, size_t host_cap, size_t *host_len,
			   int *has_port, int *port)
{
	const char *host_start;
	size_t hlen;
	const char *close;
	const char *port_str;

	if (spec == NULL || host_buf == NULL || host_cap == 0)
		return -1;

	*has_port = 0;
	*port = 0;

	/* IPv6 literal: "[...]" or "[...]:port" or "[...]:*". */
	if (spec[0] == '[') {
		close = find_byte(spec, ']');
		if (close == NULL)
			return -1;

		host_start = spec + 1;
		hlen = (size_t)(close - host_start);

		if (close[1] == '\0') {
			/* No port segment. */
		} else if (close[1] == ':') {
			port_str = close + 2;
			if (port_str[0] == '*' && port_str[1] == '\0') {
				/* wildcard, has_port stays 0 */
			} else {
				if (parse_port(port_str, port) != 0)
					return -1;
				*has_port = 1;
			}
		} else {
			return -1;
		}
	} else {
		const char *colon = find_byte(spec, ':');

		if (colon == NULL) {
			/* Bare host: no port. */
			host_start = spec;
			hlen = retrace_real_impls.strlen(spec);
		} else {
			host_start = spec;
			hlen = (size_t)(colon - spec);

			port_str = colon + 1;
			if (port_str[0] == '*' && port_str[1] == '\0') {
				/* wildcard */
			} else {
				if (parse_port(port_str, port) != 0)
					return -1;
				*has_port = 1;
			}
		}
	}

	if (hlen == 0 || hlen >= host_cap)
		return -1;

	retrace_real_impls.memcpy(host_buf, host_start, hlen);
	host_buf[hlen] = '\0';
	*host_len = hlen;
	return 0;
}

int retrace_sockaddr_match(const struct retrace_sockaddr_info *info,
			   const char *spec)
{
	char host_buf[RETRACE_SOCKADDR_IP_LEN];
	size_t host_len = 0;
	int has_port = 0;
	int port = 0;
	int host_is_wild;
	int rc;

	if (info == NULL || spec == NULL)
		return -1;

	/* Bare "*" matches anything. */
	if (spec[0] == '*' && spec[1] == '\0')
		return 1;

	/* UNIX path: no colon in spec means match by path only.
	 * A spec with a colon never matches a unix sockaddr.
	 */
	if (info->group == RETRACE_SAF_UNIX) {
		if (find_char(spec, ':'))
			return 0;
		if (retrace_real_impls.strcmp(spec, info->path) == 0)
			return 1;
		return 0;
	}

	/* For non-INET infos, a path-only spec never matches. */
	if (info->group != RETRACE_SAF_INET &&
	    info->group != RETRACE_SAF_INET6) {
		if (!find_char(spec, ':'))
			return 0;
	}

	rc = parse_host_port(spec, host_buf, sizeof(host_buf),
		&host_len, &has_port, &port);
	if (rc != 0)
		return -1;

	host_is_wild = (host_len == 1 && host_buf[0] == '*');

	if (!host_is_wild) {
		if (retrace_real_impls.strcmp(host_buf, info->ip) != 0)
			return 0;
	}

	if (has_port && info->port != (uint16_t)port)
		return 0;

	return 1;
}
