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
#include "str.h"
#include "malloc.h"
#include "plugin.h"

#include "sock.h"

#define RETRACE_MAX_IP_ADDR_LEN 15

int RETRACE_IMPLEMENTATION(connect)(int fd, const struct sockaddr *address, socklen_t len)
{
	// get connect function pointer
	real_connect = RETRACE_GET_REAL(connect);
	real_free = RETRACE_GET_REAL(free);

	// check tracing enabled
	if (!get_tracing_enabled())
		return real_connect(fd, address, len);

	// check plugin is enabled
	rtr_plugin_sock_t *sock_plugin = rtr_plugin_get(RTR_PLUGIN_TYPE_SOCK);
	if (sock_plugin && sock_plugin->p_connect)
		return sock_plugin->p_connect(fd, address, len);

	// check socket family
	if (address->sa_family == AF_INET)
	{
		char *match_ip = NULL, *redirect_ip = NULL;
		int match_port, redirect_port;

		const char *dst_ipaddr = inet_ntoa(((struct sockaddr_in *) address)->sin_addr);
		int dst_port = ntohs(((struct sockaddr_in *) address)->sin_port);

		rtr_config config = NULL;

		trace_printf(1, "connect(%d, %s:%d, %d), [%s]\n", fd, dst_ipaddr, dst_port, len, "AF_INET");

		while (rtr_get_config_multiple(&config, "connect",
				ARGUMENT_TYPE_STRING,
				ARGUMENT_TYPE_INT,
				ARGUMENT_TYPE_STRING,
				ARGUMENT_TYPE_INT,
				ARGUMENT_TYPE_END,
				&match_ip,
				&match_port,
				&redirect_ip,
				&redirect_port))
		{
			trace_printf(1, "try matching with config 'match_addr-%s:%d, redirect_addr-%s:%d'\n",
							match_ip, match_port, redirect_ip, redirect_port);

			// check IP address and port number is matched
			if (strcmp(match_ip, dst_ipaddr) == 0 && match_port == dst_port)
			{
				struct sockaddr_in redirect_addr;

				// set redirect address
				memset(&redirect_addr, 0, sizeof(redirect_addr));

				redirect_addr.sin_family = AF_INET;
				redirect_addr.sin_addr.s_addr = inet_addr(redirect_ip);
				redirect_addr.sin_port = htons(redirect_port);

				trace_printf(1, "redirect connect(%d, %s:%d, %d), [%s]\n", fd, redirect_ip, redirect_port,
								sizeof(struct sockaddr_in), "AF_INET");

				// update file descriptor
				file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_IPV4_CONNECT, redirect_ip, redirect_port);

				// free buffers
				real_free(match_ip);
				real_free(redirect_ip);

				rtr_confing_close(config);

				return real_connect(fd, (struct sockaddr *) &redirect_addr, sizeof(redirect_addr));
			}

			// free buffers
			if (match_ip)
				real_free(match_ip);

			if (redirect_ip)
				real_free(redirect_ip);

			match_ip = redirect_ip = NULL;
		}

		// close config
		if (config)
		{
			rtr_confing_close(config);
			config = NULL;
		}
	}

	return real_connect(fd, address, len);
}

RETRACE_REPLACE(connect)

int RETRACE_IMPLEMENTATION(bind)(int fd, const struct sockaddr *address, socklen_t len)
{
	real_bind = RETRACE_GET_REAL(bind);

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

int RETRACE_IMPLEMENTATION(accept)(int fd, struct sockaddr *address, socklen_t *len)
{
	real_accept = RETRACE_GET_REAL(accept);
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

int RETRACE_IMPLEMENTATION(atoi)(const char *str)
{
	real_atoi = RETRACE_GET_REAL(atoi);
	trace_printf(1, "atoi(%s);\n", str);
	return real_atoi(str);
}

RETRACE_REPLACE(atoi)

ssize_t RETRACE_IMPLEMENTATION(send)(int fd, const void *buf, size_t len, int flags)
{
	real_send = RETRACE_GET_REAL(send);

	// check tracing enabled
	if (!get_tracing_enabled())
		return real_send(fd, buf, len, flags);

	// check plugin is enabled
	rtr_plugin_sock_t *sock_plugin = rtr_plugin_get(RTR_PLUGIN_TYPE_SOCK);
	if (sock_plugin && sock_plugin->p_send)
		return sock_plugin->p_send(fd, buf, len, flags);

	return real_send(fd, buf, len, flags);
}

RETRACE_REPLACE(send)

ssize_t RETRACE_IMPLEMENTATION(recv)(int fd, void *buf, size_t len, int flags)
{
	real_recv = RETRACE_GET_REAL(recv);

	// check tracing enabled
	if (!get_tracing_enabled())
		return real_recv(fd, buf, len, flags);

	// check plugin is enabled
	rtr_plugin_sock_t *sock_plugin = rtr_plugin_get(RTR_PLUGIN_TYPE_SOCK);
	if (sock_plugin && sock_plugin->p_recv)
		return sock_plugin->p_recv(fd, buf, len, flags);

	return real_recv(fd, buf, len, flags);
}

RETRACE_REPLACE(recv)
