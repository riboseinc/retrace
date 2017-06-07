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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"
#include "sock.h"
#include <arpa/inet.h>

int inet_pton(int af, const char *src, void *dst);

int
RETRACE_IMPLEMENTATION(connect)(int fd, const struct sockaddr *address, socklen_t len)
{
	char *redirect_ip = NULL;
	char *match_ip = NULL;
	int match_port;
	int redirect_port;
	unsigned short port = ntohs(*(unsigned short *)&address->sa_data[0]);

	real_connect = dlsym(RTLD_NEXT, "connect");

	// Only implemented for IPv4 right now
	if (get_tracing_enabled() && address->sa_family == AF_INET &&
	    get_redirect("connect",
			 ARGUMENT_TYPE_STRING, // match_ip
			 ARGUMENT_TYPE_INT,    // match_port
			 ARGUMENT_TYPE_STRING, // redirect_ip
			 ARGUMENT_TYPE_INT,    // redirect_port
			 ARGUMENT_TYPE_END,
			 &match_ip,
			 &match_port,
			 &redirect_ip,
			 &redirect_port)) {
		struct sockaddr match_addr;

		// Convert the ip address and port to a struct sockaddr to compare
		// if we want to redirect this connection
		match_addr.sa_family = address->sa_family;
		*((unsigned short *) &match_addr.sa_data[0]) = htons(match_port);
		inet_pton(AF_INET, match_ip, (struct in_addr *) &match_addr.sa_data[2]);

		if (match_addr.sa_data[0] == address->sa_data[0] &&
		    match_addr.sa_data[1] == address->sa_data[1] &&
		    match_addr.sa_data[2] == address->sa_data[2] &&
		    match_addr.sa_data[3] == address->sa_data[3] &&
		    match_addr.sa_data[4] == address->sa_data[4] &&
		    match_addr.sa_data[5] == address->sa_data[5]) {
			// We have a match! Construct a struct sockaddr of where we want to
			// redirect to.
			struct sockaddr redirect_addr;

			redirect_addr.sa_family = address->sa_family;
			*((unsigned short *) &redirect_addr.sa_data[0]) = htons(redirect_port);
			inet_pton(
			  AF_INET, redirect_ip, (struct in_addr *) &redirect_addr.sa_data[2]);


			trace_printf(
			  1,
			  "connect(%d, \"%hu.%hu.%hu.%hu:%u\", %zu); [redirection in effect: "
			  "\"%hu.%hu.%hu.%hu:%u\"]\n",
			  fd,
			  (unsigned short) address->sa_data[2] & 0xFF,
			  (unsigned short) address->sa_data[3] & 0xFF,
			  (unsigned short) address->sa_data[4] & 0xFF,
			  (unsigned short) address->sa_data[5] & 0xFF,
			  port,
			  len,
			  (unsigned short) redirect_addr.sa_data[2] & 0xFF,
			  (unsigned short) redirect_addr.sa_data[3] & 0xFF,
			  (unsigned short) redirect_addr.sa_data[4] & 0xFF,
			  (unsigned short) redirect_addr.sa_data[5] & 0xFF,
			  redirect_port);

            // cleanup
            free(redirect_ip);
            free(match_ip);

			return real_connect(fd, &redirect_addr, len);
		}
	}

	trace_printf(1,
		     "connect(%d, \"%hu.%hu.%hu.%hu:%u\", %zu);\n",
		     fd,
		     (unsigned short) address->sa_data[2] & 0xFF,
		     (unsigned short) address->sa_data[3] & 0xFF,
		     (unsigned short) address->sa_data[4] & 0xFF,
		     (unsigned short) address->sa_data[5] & 0xFF,
		     port,
		     len);

    // cleanup
    free(redirect_ip);
    free(match_ip);

	return real_connect(fd, address, len);
}

RETRACE_REPLACE(connect)

int
RETRACE_IMPLEMENTATION(bind)(int fd, const struct sockaddr *address, socklen_t len)
{
	real_bind = dlsym(RTLD_NEXT, "bind");

	trace_printf(1,
		     "bind(%d, \"%hu.%hu.%hu.%hu:%hu\", %zu);\n",
		     fd,
		     (unsigned short) address->sa_data[2] & 0xFF,
		     (unsigned short) address->sa_data[3] & 0xFF,
		     (unsigned short) address->sa_data[4] & 0xFF,
		     (unsigned short) address->sa_data[5] & 0xFF,
		     (256 * address->sa_data[0]) + address->sa_data[1],
		     len);

	return real_bind(fd, address, len);
}

RETRACE_REPLACE(bind)

int
RETRACE_IMPLEMENTATION(accept)(int fd, struct sockaddr *address, socklen_t *len)
{
	real_accept = dlsym(RTLD_NEXT, "accept");
	trace_printf(1,
		     "accept(%d, \"%hu.%hu.%hu.%hu:%hu\", %zu);\n",
		     fd,
		     (unsigned short) address->sa_data[2] & 0xFF,
		     (unsigned short) address->sa_data[3] & 0xFF,
		     (unsigned short) address->sa_data[4] & 0xFF,
		     (unsigned short) address->sa_data[5] & 0xFF,
		     (256 * address->sa_data[0]) + address->sa_data[1],
		     *len);

	return real_accept(fd, address, len);
}

RETRACE_REPLACE(accept)

int
RETRACE_IMPLEMENTATION(atoi)(const char *str)
{
	real_atoi = dlsym(RTLD_NEXT, "atoi");
	trace_printf(1, "atoi(%s);\n", str);
	return real_atoi(str);
}

RETRACE_REPLACE(atoi)
