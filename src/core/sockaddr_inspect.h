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
 * sockaddr_inspect -- uniform view of sockaddr* across families.
 *
 * The BSD sockets API passes `struct sockaddr *` everywhere, but the
 * actual layout depends on `sa_family` (AF_INET -> sockaddr_in,
 * AF_INET6 -> sockaddr_in6, AF_UNIX -> sockaddr_un). Actions that
 * want to reason about a peer address (deny-by-destination, audit
 * log, redirect) need a uniform view that does not require every
 * caller to know the union shape.
 *
 * This helper reads `sa_family` from the first 2 bytes, dispatches
 * on the family, and fills a flat info struct. For unsupported
 * families the helper still returns 0 but leaves `family` set and
 * the other fields zeroed -- callers can branch on `family`.
 *
 * Recursion safety: every libc call inside this module goes through
 * retrace_real_impls. We never call inet_ntop/inet_pton directly,
 * so intercepting those symbols (TODO.complete/15) cannot recurse
 * back into us.
 */

#ifndef RETRACE_CORE_SOCKADDR_INSPECT_H_
#define RETRACE_CORE_SOCKADDR_INSPECT_H_

#include <stddef.h>
#include <stdint.h>

#define RETRACE_SOCKADDR_IP_LEN   64   /* fits IPv6 + NUL */
#define RETRACE_SOCKADDR_PATH_LEN 108  /* matches sun_path */

enum retrace_sockaddr_family_group {
	RETRACE_SAF_UNKNOWN = 0,
	RETRACE_SAF_INET    = 1,
	RETRACE_SAF_INET6   = 2,
	RETRACE_SAF_UNIX    = 3,
};

struct retrace_sockaddr_info {
	/* Raw sa_family value (AF_INET, AF_INET6, AF_UNIX, ...). */
	int family;

	/* Normalized group so actions don't need AF_* constants. */
	enum retrace_sockaddr_family_group group;

	/* Expected payload size for this family (sizeof(sockaddr_in)
	 * etc.). 0 if unknown.
	 */
	size_t expected_len;

	/* AF_INET / AF_INET6: presentation-form address ("1.2.3.4"
	 * or "fe80::1"). Empty string for other families or when
	 * the address could not be formatted.
	 */
	char ip[RETRACE_SOCKADDR_IP_LEN];

	/* AF_INET / AF_INET6: port in host byte order. 0 if unknown. */
	uint16_t port;

	/* AF_UNIX: filesystem path. May be empty for anonymous unix
	 * sockets. Empty for other families.
	 */
	char path[RETRACE_SOCKADDR_PATH_LEN];
};

/*
 * Inspect a sockaddr pointer.
 *
 *   sa       - the sockaddr* (must not be NULL)
 *   addrlen  - byte count available at *sa; use sizeof(sockaddr_in)
 *              etc. 0 means "unknown, fall back to expected_len."
 *   out      - filled on return; never NULL.
 *
 * Returns 0 on success (out is populated, possibly with group=UNKNOWN
 * for an unsupported family), -1 on hard failure (NULL sa/out, or
 * addrlen too small for the family header).
 */
int retrace_sockaddr_inspect(const void *sa, size_t addrlen,
			      struct retrace_sockaddr_info *out);

/*
 * Match an inspected sockaddr against a "host:port" spec.
 *
 * Spec grammar (each part optional, but the ':' separator is
 * required when port is present):
 *
 *   "1.2.3.4:443"   -- exact IPv4 + port
 *   "1.2.3.4:*"     -- IPv4, any port
 *   "*:443"         -- any host, port 443
 *   "*:*" or "*"    -- match anything (semantic "deny all")
 *   "[::1]:443"     -- IPv6 literal (brackets mandatory when port given)
 *   "[::1]:*"       -- IPv6, any port
 *   "/var/run/x"    -- AF_UNIX path (no host:port); exact path match
 *
 * For AF_UNIX infos, only the path form matches; "host:port" specs
 * never match a unix sockaddr (and vice versa).
 *
 * Returns 1 on match, 0 on no match, -1 on malformed spec.
 */
int retrace_sockaddr_match(const struct retrace_sockaddr_info *info,
			   const char *spec);

#endif /* RETRACE_CORE_SOCKADDR_INSPECT_H_ */
