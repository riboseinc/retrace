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

#include "common.h"
#include "rtr-netdb.h"

#include <arpa/inet.h>

struct hostent *RETRACE_IMPLEMENTATION(gethostbyname)(const char *name)
{
	struct hostent *hent;

	hent = real_gethostbyname(name);
	if (hent) {
		int i;

		trace_printf(1, "gethostbyname(\"%s\") = [", name);

		for (i = 0; hent->h_addr_list[i] != NULL; i++) {
			char ip_addr[INET6_ADDRSTRLEN];

			inet_ntop(hent->h_addrtype, hent->h_addr_list[i], ip_addr, sizeof(ip_addr));
			trace_printf(0, i > 0 ? ",%s" : "%s", ip_addr);
		}

		trace_printf(0, "]\n");
	} else
		trace_printf(1, "gethostbyname(\"%s\") = NULL [error:%s]\n", name, hstrerror(h_errno));

	return hent;
}

RETRACE_REPLACE(gethostbyname, struct hostent *, (const char *name), (name))


struct hostent *RETRACE_IMPLEMENTATION(gethostbyaddr)(const void *addr, socklen_t len, int type)
{
	struct hostent *hent;

	char ip_addr[INET6_ADDRSTRLEN];

	/* get IP address */
	inet_ntop(type, addr, ip_addr, sizeof(ip_addr));

	hent = real_gethostbyaddr(addr, len, type);
	if (hent)
		trace_printf(1, "gethostbyaddr(IP => %s, , type => %s) = [Hostname:%s]\n", ip_addr,
			type == AF_INET ? "AF_INET" : "AF_INET6",
			hent->h_name);
	else
		trace_printf(1, "gethostbyaddr(IP => %s, , type => %s) = [Error:%s]\n", ip_addr,
			type == AF_INET ? "AF_INET" : "AF_INET6",
			hstrerror(h_errno));

	return hent;
}

RETRACE_REPLACE(gethostbyaddr, struct hostent *,
	(const void *addr, socklen_t len, int type), (addr, len, type))


void RETRACE_IMPLEMENTATION(sethostent)(int stayopen)
{
	trace_printf(1, "sethostent(%d)\n", stayopen);

	real_sethostent(stayopen);
}

RETRACE_REPLACE(sethostent, void, (int stayopen), (stayopen))


void RETRACE_IMPLEMENTATION(endhostent)(void)
{
	trace_printf(1, "endhostent()\n");

	real_endhostent();
}

RETRACE_REPLACE(endhostent, void, (void), ())


struct hostent *RETRACE_IMPLEMENTATION(gethostent)(void)
{
	struct hostent *hent;

	hent = real_gethostent();
	if (hent) {
		int i;

		trace_printf(1, "gethostent() = [");

		for (i = 0; hent->h_addr_list[i] != NULL; i++) {
			char ip_addr[INET6_ADDRSTRLEN];

			inet_ntop(hent->h_addrtype, hent->h_addr_list[i], ip_addr, sizeof(ip_addr));
			trace_printf(0, i > 0 ? ",%s" : "%s", ip_addr);
		}

		trace_printf(0, "]\n");
	} else
		trace_printf(1, "gethostent() = NULL [error:%s]\n", hstrerror(h_errno));

	return hent;
}

RETRACE_REPLACE(gethostent, struct hostent *, (void), ())


struct hostent *RETRACE_IMPLEMENTATION(gethostbyname2)(const char *name, int af)
{
	struct hostent *hent;

	char ip_addr[INET6_ADDRSTRLEN];
	int i;

	hent = real_gethostbyname2(name, af);
	if (hent) {
		int i;

		trace_printf(1, "gethostbyname2(\"%s\", %s) = [", name, af == AF_INET ? "AF_INET" : "AF_INET6");

		for (i = 0; hent->h_addr_list[i] != NULL; i++) {
			char ip_addr[INET6_ADDRSTRLEN];

			inet_ntop(hent->h_addrtype, hent->h_addr_list[i], ip_addr, sizeof(ip_addr));
			trace_printf(0, i > 0 ? ",%s" : "%s", ip_addr);
		}

		trace_printf(0, "]\n");
	} else
		trace_printf(1, "gethostbyname2(%s, %d) = NULL [error:%s]\n", name, af, hstrerror(h_errno));

	return hent;
}

RETRACE_REPLACE(gethostbyname2, struct hostent *, (const char *name, int af),
	(name, af))


int RETRACE_IMPLEMENTATION(getaddrinfo)(const char *node, const char *service,
	const struct addrinfo *hints, struct addrinfo **res)
{
	int ret;

	ret = real_getaddrinfo(node, service, hints, res);
	if (ret == 0) {
		struct addrinfo *rp, *result = *res;

		trace_printf(1, "getaddrinfo(\"%s\", \"%s\", hints=>[AF_FAMILY:%d, SOCK_TYPE:%d], ) = [%p=",
			node ? node : "NULL", service ? service : "NULL",
			hints->ai_family, hints->ai_socktype, *res);

		for (rp = result; rp != NULL; rp = rp->ai_next) {
			char addr[INET6_ADDRSTRLEN];

			if (rp->ai_family == AF_INET) {
				struct sockaddr_in *in_addr = (struct sockaddr_in *)rp->ai_addr;

				inet_ntop(rp->ai_family, &(in_addr->sin_addr), addr, sizeof(addr));
			} else if (rp->ai_family == AF_INET6) {
				struct sockaddr_in6 *in_addr = (struct sockaddr_in6 *)rp->ai_addr;

				inet_ntop(rp->ai_family, &(in_addr->sin6_addr), addr, sizeof(addr));
			}

			trace_printf(0, (rp != result) ? ",%s" : "%s", addr);
		}

		trace_printf(0, "]\n");
	} else
		trace_printf(1, "getaddrinfo(\"%s\", \"%s\", [AF_FAMILY:%d, SOCK_TYPE:%d]) = [Error:%s]\n",
			node ? node : "NULL", service ? service : "NULL",
			hints ? hints->ai_family : -1, hints ? hints->ai_socktype : -1,
			gai_strerror(ret));

	return ret;
}

RETRACE_REPLACE(getaddrinfo, int,
	(const char *node, const char *service, const struct addrinfo *hints,
	    struct addrinfo **res),
	(node, service, hints, res))


void RETRACE_IMPLEMENTATION(freeaddrinfo)(struct addrinfo *res)
{
	trace_printf(1, "freeaddrinfo(%p)\n", res);
	real_freeaddrinfo(res);
}

RETRACE_REPLACE(freeaddrinfo, void, (struct addrinfo *res), (res))
