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

#define RETRACE_MAX_IP_ADDR_LEN 15

int RETRACE_IMPLEMENTATION(socket)(int domain, int type, int protocol)
{
	int sock;
	rtr_socket_t real_socket;

	real_socket = RETRACE_GET_REAL(socket);

	sock = real_socket(domain, type, protocol);
	trace_printf(1, "socket(%d, %d, %d) [return: %d]\n", domain, type, protocol, sock);

	return sock;
}

RETRACE_REPLACE(socket)

#ifdef __linux__
int RETRACE_IMPLEMENTATION(connect)(int fd, __CONST_SOCKADDR_ARG _address, socklen_t len)
#else
int RETRACE_IMPLEMENTATION(connect)(int fd, const struct sockaddr *address, socklen_t len)
#endif
{
	rtr_connect_t real_connect;
#ifdef __linux__
	const struct sockaddr *address = _address.__sockaddr__;
#endif

	int ret;

	real_connect = RETRACE_GET_REAL(connect);

	if (!get_tracing_enabled())
		return real_connect(fd, address, len);

	if (address->sa_family == AF_INET) {
		struct sockaddr_in *dst_addr = (struct sockaddr_in *) address;

		const char *dst_ipaddr = inet_ntoa(dst_addr->sin_addr);
		int dst_port = ntohs(dst_addr->sin_port);

		/* connect to remote */
		ret = real_connect(fd, (struct sockaddr *) dst_addr, sizeof(struct sockaddr_in));
		if (ret == 0)
			file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_IPV4_CONNECT, dst_ipaddr, dst_port);

		trace_printf(1, "connect(%d, %s:%d, ); [%d](AF_INET)\n", fd, dst_ipaddr, dst_port, ret);

		return ret;
	} else if (address->sa_family == AF_UNIX) {
		struct sockaddr_un *un_addr = (struct sockaddr_un *) address;
		const char *sun_path = un_addr->sun_path;

		/* connect to local */
		ret = real_connect(fd, (struct sockaddr *) un_addr, sizeof(struct sockaddr_un));
		if (ret == 0)
			file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_UNIX_DOMAIN, sun_path, -1);

		trace_printf(1, "connect(%d, \"%s\", ); [%d](AF_UNIX|AF_LOCAL)\n", fd, sun_path, ret);

		return ret;
	}

	trace_printf(1, "connect(%d, sa_family:%d)\n", fd, address->sa_family);

	return real_connect(fd, address, len);
}

RETRACE_REPLACE(connect)

#ifdef __linux__
int RETRACE_IMPLEMENTATION(bind)(int fd, __CONST_SOCKADDR_ARG _address, socklen_t len)
#else
int RETRACE_IMPLEMENTATION(bind)(int fd, const struct sockaddr *address, socklen_t len)
#endif
{
	rtr_bind_t real_bind;
#ifdef __linux__
	const struct sockaddr *address = _address.__sockaddr__;
#endif

	int ret;

	real_bind = RETRACE_GET_REAL(bind);

	if (address->sa_family == AF_INET) {
		struct sockaddr_in *bind_addr = (struct sockaddr_in *) address;

		const char *bind_ipaddr = inet_ntoa(bind_addr->sin_addr);
		int bind_port = ntohs(bind_addr->sin_port);

		/* connect to remote */
		ret = real_bind(fd, (struct sockaddr *) bind_addr, sizeof(struct sockaddr_in));
		if (ret == 0)
			file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_IPV4_BIND, bind_ipaddr, bind_port);

		trace_printf(1, "bind(%d, %s:%d, ); [%d](AF_INET)\n", fd, bind_ipaddr, bind_port, ret);

		return ret;
	} else if (address->sa_family == AF_UNIX) {
		struct sockaddr_un *un_addr = (struct sockaddr_un *) address;
		const char *sun_path = un_addr->sun_path;

		/* connect to local */
		ret = real_bind(fd, (struct sockaddr *) un_addr, sizeof(struct sockaddr_un));
		if (ret == 0)
			file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_IPV4_BIND, sun_path, -1);

		trace_printf(1, "bind(%d, \"%s\", ); [%d](AF_UNIX|AF_LOCAL)\n", fd, sun_path, ret);

		return ret;
	}

	return real_bind(fd, address, len);
}

RETRACE_REPLACE(bind)

#ifdef __linux__
int RETRACE_IMPLEMENTATION(accept)(int fd, __SOCKADDR_ARG _address, socklen_t *len)
#else
int RETRACE_IMPLEMENTATION(accept)(int fd, struct sockaddr *address, socklen_t *len)
#endif
{
	rtr_accept_t real_accept;
#ifdef __linux__
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
