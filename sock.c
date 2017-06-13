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
#include "sock.h"
#include <arpa/inet.h>

#define RETRACE_MAX_IP_ADDR_LEN 15
#ifdef __APPLE__
int RETRACE_IMPLEMENTATION(connect)(int fd, const struct sockaddr *address, socklen_t len)
#else
int RETRACE_IMPLEMENTATION(connect)(int fd, __CONST_SOCKADDR_ARG _address, socklen_t len)
#endif
{
	unsigned short port;
	int match_port;
	int redirect_port;
	char *match_ip = NULL;
	char *redirect_ip = NULL;
	rtr_connect_t real_connect;
	char ip_address[RETRACE_MAX_IP_ADDR_LEN + 1];
#ifndef __APPLE__
	const struct sockaddr *address = _address.__sockaddr__;
#endif

	port = ntohs(*(unsigned short *)&address->sa_data[0]);

	real_connect = RETRACE_GET_REAL(connect);

	/* Only implemented for IPv4 right now */
	if (get_tracing_enabled() && address->sa_family == AF_INET) {
		FILE *config = NULL;

		while (1) {
			int ret;
			struct sockaddr match_addr;

			ret = rtr_get_config_multiple(&config, "connect",
					ARGUMENT_TYPE_STRING, /* match_ip */
					ARGUMENT_TYPE_INT,    /* match_port */
					ARGUMENT_TYPE_STRING, /* redirect_ip */
					ARGUMENT_TYPE_INT,    /* redirect_port */
					ARGUMENT_TYPE_END,
					&match_ip,
					&match_port,
					&redirect_ip,
					&redirect_port);
			if (ret == 0)
				break;

			/*
			 * Convert the ip address and port to a struct sockaddr to compare
			 * if we want to redirect this connection
			 */
			match_addr.sa_family = address->sa_family;
			*((unsigned short *)&match_addr.sa_data[0]) = htons(match_port);
			inet_pton(AF_INET, match_ip, (struct in_addr *)&match_addr.sa_data[2]);

			if (match_addr.sa_data[0] == address->sa_data[0] &&
			    match_addr.sa_data[1] == address->sa_data[1] &&
			    match_addr.sa_data[2] == address->sa_data[2] &&
			    match_addr.sa_data[3] == address->sa_data[3] &&
			    match_addr.sa_data[4] == address->sa_data[4] &&
			    match_addr.sa_data[5] == address->sa_data[5]) {
				/*
				 * We have a match! Construct a struct sockaddr of where we want to
				 * redirect to.
				 */
				struct sockaddr redirect_addr;

				redirect_addr.sa_family = address->sa_family;
				*((unsigned short *)&redirect_addr.sa_data[0]) = htons(redirect_port);
				inet_pton(AF_INET, redirect_ip, (struct in_addr *)&redirect_addr.sa_data[2]);

				trace_printf(
						1,
						"connect(%d, \"%hu.%hu.%hu.%hu:%u\", %zu); [redirection in effect: "
						"\"%hu.%hu.%hu.%hu:%u\"]\n",
						fd,
						(unsigned short)address->sa_data[2] & 0xFF,
						(unsigned short)address->sa_data[3] & 0xFF,
						(unsigned short)address->sa_data[4] & 0xFF,
						(unsigned short)address->sa_data[5] & 0xFF,
						port,
						len,
						(unsigned short)redirect_addr.sa_data[2] & 0xFF,
						(unsigned short)redirect_addr.sa_data[3] & 0xFF,
						(unsigned short)redirect_addr.sa_data[4] & 0xFF,
						(unsigned short)redirect_addr.sa_data[5] & 0xFF,
						redirect_port);

				file_descriptor_update(
						fd, FILE_DESCRIPTOR_TYPE_IPV4_CONNECT, redirect_ip, redirect_port);

				/* cleanup */
				free(redirect_ip);
				free(match_ip);

				rtr_confing_close(config);

				return real_connect(fd, &redirect_addr, len);
			}

			if (redirect_ip) {
				free(redirect_ip);
				redirect_ip = NULL;
			}

			if (match_ip) {
				free(match_ip);
				match_ip = NULL;
			}
		}

		if (config)
			rtr_confing_close(config);
	}

	snprintf(ip_address,
			RETRACE_MAX_IP_ADDR_LEN,
			"%d.%d.%d.%d",
			(int)address->sa_data[2] & 0xFF,
			(int)address->sa_data[3] & 0xFF,
			(int)address->sa_data[4] & 0xFF,
			(int)address->sa_data[5] & 0xFF);

	trace_printf(1, "connect(%d, \"%s\", %zu);\n", fd, ip_address, port, len);

	file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_IPV4_CONNECT, ip_address, port);

	return real_connect(fd, address, len);
}

RETRACE_REPLACE(connect)

#ifdef __APPLE__
int RETRACE_IMPLEMENTATION(bind)(int fd, const struct sockaddr *address, socklen_t len)
#else
int RETRACE_IMPLEMENTATION(bind)(int fd, __CONST_SOCKADDR_ARG _address, socklen_t len)
#endif
{
	rtr_bind_t real_bind;
#ifndef __APPLE__
	const struct sockaddr *address = _address.__sockaddr__;
#endif

	real_bind = RETRACE_GET_REAL(bind);

	trace_printf(1,
			"bind(%d, \"%hu.%hu.%hu.%hu:%hu\", %zu);\n",
			fd,
			(unsigned short)address->sa_data[2] & 0xFF,
			(unsigned short)address->sa_data[3] & 0xFF,
			(unsigned short)address->sa_data[4] & 0xFF,
			(unsigned short)address->sa_data[5] & 0xFF,
			(256 * address->sa_data[0]) + address->sa_data[1],
			len);

	return real_bind(fd, address, len);
}

RETRACE_REPLACE(bind)

#ifdef __APPLE__
int RETRACE_IMPLEMENTATION(accept)(int fd, struct sockaddr *address, socklen_t *len)
#else
int RETRACE_IMPLEMENTATION(accept)(int fd, __SOCKADDR_ARG _address, socklen_t *len)
#endif
{
	rtr_accept_t real_accept;
#ifndef __APPLE__
	struct sockaddr *address = _address.__sockaddr__;
#endif

	real_accept = RETRACE_GET_REAL(accept);

	trace_printf(1,
			"accept(%d, \"%hu.%hu.%hu.%hu:%hu\", %zu);\n",
			fd,
			(unsigned short)address->sa_data[2] & 0xFF,
			(unsigned short)address->sa_data[3] & 0xFF,
			(unsigned short)address->sa_data[4] & 0xFF,
			(unsigned short)address->sa_data[5] & 0xFF,
			(256 * address->sa_data[0]) + address->sa_data[1],
			*len);

	return real_accept(fd, address, len);
}

RETRACE_REPLACE(accept)

int RETRACE_IMPLEMENTATION(atoi)(const char *str)
{
	rtr_atoi_t real_atoi;

	real_atoi = RETRACE_GET_REAL(atoi);

	trace_printf(1, "atoi(%s);\n", str);

	return real_atoi(str);
}

RETRACE_REPLACE(atoi)
