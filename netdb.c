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

#include <arpa/inet.h>

#include "common.h"
#include "rtr-netdb.h"

struct hostent *RETRACE_IMPLEMENTATION(gethostbyname)(const char *name)
{
	rtr_gethostbyname_t real_gethostbyname;
	struct hostent *hent;

	real_gethostbyname = RETRACE_GET_REAL(gethostbyname);

	hent = real_gethostbyname(name);
	if (hent) {
		struct in_addr **addr_list;
		int i;

		trace_printf(1, "gethostbyname(\"%s\") = [", name);

		addr_list = (struct in_addr **) hent->h_addr_list;
		for (i = 0; addr_list[i] != NULL; i++) {
			char ip_addr[INET6_ADDRSTRLEN];

			inet_ntop(hent->h_addrtype, addr_list[i], ip_addr, sizeof(ip_addr));
			if (i > 0)
				trace_printf(0, ",");
			trace_printf(0, "%s", ip_addr);
		}

		trace_printf(0, "]\n");
	} else
		trace_printf(1, "gethostbyname(\"%s\") = NULL [error:%s]\n", name, hstrerror(h_errno));

	return hent;
}

RETRACE_REPLACE(gethostbyname);

struct hostent *RETRACE_IMPLEMENTATION(gethostbyaddr)(const void *addr, socklen_t len, int type)
{
	rtr_gethostbyaddr_t real_gethostbyaddr;
	struct hostent *hent;

	real_gethostbyaddr = RETRACE_GET_REAL(gethostbyaddr);

	hent = real_gethostbyaddr(addr, len, type);
	if (addr) {
		char ip_addr[INET6_ADDRSTRLEN];

		if (type == AF_INET) {
			struct in_addr *in_addr = (struct in_addr *) addr;

			inet_ntop(AF_INET, in_addr, ip_addr, sizeof(ip_addr));
		} else if (type == AF_INET6) {
			struct in6_addr *in6_addr = (struct in6_addr *) addr;

			inet_ntop(AF_INET, in6_addr, ip_addr, sizeof(ip_addr));
		}

		if (hent)
			trace_printf(1, "gethostbyaddr(IP => %s, , type => %s) = [Hostname:%s]\n", ip_addr,
				type == AF_INET ? "AF_INET" : "AF_INET6",
				hent->h_name);
		else
			trace_printf(1, "gethostbyaddr(IP => %s, , type => %s) = [Error:%s]\n", ip_addr,
				type == AF_INET ? "AF_INET" : "AF_INET6",
				hstrerror(h_errno));
	}

	return hent;
}

RETRACE_REPLACE(gethostbyaddr);

void RETRACE_IMPLEMENTATION(sethostent)(int stayopen)
{
	rtr_sethostent_t real_sethostent;

	real_sethostent = RETRACE_GET_REAL(sethostent);

	trace_printf(1, "sethostent(%d)\n", stayopen);

	return real_sethostent(stayopen);
}

RETRACE_REPLACE(sethostent);

void RETRACE_IMPLEMENTATION(endhostent)(void)
{
	rtr_endhostent_t real_endhostent;

	real_endhostent = RETRACE_GET_REAL(endhostent);

	trace_printf(1, "endhostent()\n");

	return real_endhostent();
}

RETRACE_REPLACE(endhostent);

struct hostent *RETRACE_IMPLEMENTATION(gethostent)(void)
{
	rtr_gethostent_t real_gethostent;
	struct hostent *hent;

	real_gethostent = RETRACE_GET_REAL(gethostent);

	hent = real_gethostent();
	if (hent) {
		struct in_addr **addr_list;
		int i;

		trace_printf(1, "gethostent() = [");

		addr_list = (struct in_addr **) hent->h_addr_list;
		for (i = 0; addr_list[i] != NULL; i++) {
			char ip_addr[INET6_ADDRSTRLEN];

			inet_ntop(hent->h_addrtype, addr_list[i], ip_addr, sizeof(ip_addr));
			if (i > 0)
				trace_printf(0, ",");
			trace_printf(0, "%s", ip_addr);
		}

		trace_printf(0, "]\n");
	} else
		trace_printf(1, "gethostent() = NULL [error:%s]\n", hstrerror(h_errno));

	return hent;
}

RETRACE_REPLACE(gethostent);

struct hostent *RETRACE_IMPLEMENTATION(gethostbyname2)(const char *name, int af)
{
	rtr_gethostbyname2_t real_gethostbyname2;
	struct hostent *hent;

	real_gethostbyname2 = RETRACE_GET_REAL(gethostbyname2);

	hent = real_gethostbyname2(name, af);
	if (hent) {
		trace_printf(1, "gethostbyname2(\"%s\", %s) = [", name, af == AF_INET ? "AF_INET" : "AF_INET6");

		if (af == AF_INET) {
			struct in_addr **addr_list;
			int i;

			addr_list = (struct in_addr **) hent->h_addr_list;
			for (i = 0; addr_list[i] != NULL; i++) {
				char ip_addr[INET6_ADDRSTRLEN];

				inet_ntop(hent->h_addrtype, addr_list[i], ip_addr, sizeof(ip_addr));
				if (i > 0)
					trace_printf(0, ",");
				trace_printf(0, "%s", ip_addr);
			}
		} else if (af == AF_INET6) {
			struct in6_addr **addr_list;
			int i;

			addr_list = (struct in6_addr **) hent->h_addr_list;
			for (i = 0; addr_list[i] != NULL; i++) {
				char ip_addr[INET6_ADDRSTRLEN];

				inet_ntop(hent->h_addrtype, addr_list[i], ip_addr, sizeof(ip_addr));
				if (i > 0)
					trace_printf(0, ",");
				trace_printf(0, "%s", ip_addr);
			}
		}

		trace_printf(0, "]\n");
	} else
		trace_printf(1, "gethostbyname2(%s, %d) = NULL [error:%s]\n", name, af, hstrerror(h_errno));

	return hent;
}

RETRACE_REPLACE(gethostbyname2);

int RETRACE_IMPLEMENTATION(getaddrinfo)(const char *node, const char *service,
	const struct addrinfo *hints, struct addrinfo **res)
{
	rtr_getaddrinfo_t real_getaddrinfo;
	int ret;

	real_getaddrinfo = RETRACE_GET_REAL(getaddrinfo);

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

			if (rp != result)
				trace_printf(0, ",");

			trace_printf(0, "%s", addr);
		}

		trace_printf(0, "]\n");
	} else
		trace_printf(1, "getaddrinfo(\"%s\", \"%s\", [AF_FAMILY:%d, SOCK_TYPE:%d]) = [Error:%s]\n",
			node ? node : "NULL", service ? service : "NULL",
			hints ? hints->ai_family : -1, hints ? hints->ai_socktype : -1,
			gai_strerror(ret));

	return ret;
}

RETRACE_REPLACE(getaddrinfo);

void RETRACE_IMPLEMENTATION(freeaddrinfo)(struct addrinfo *res)
{
	rtr_freeaddrinfo_t real_freeaddrinfo;

	real_freeaddrinfo = RETRACE_GET_REAL(freeaddrinfo);

	trace_printf(1, "freeaddrinfo(%p)\n", res);
	real_freeaddrinfo(res);
}

RETRACE_REPLACE(freeaddrinfo);
