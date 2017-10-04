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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "frontend.h"
#include "display.h"
#include "dns.h"
#include "handlers.h"

#define IN_PORT(addr) (((struct sockaddr_in *)(addr))->sin_port)
#define IN6_PORT(addr) (((struct sockaddr_in6 *)(addr))->sin6_port)

#if defined __APPLE__ || defined __FreeBSD__
#define IS_DNS_DOMAIN(d) ((d) == PF_INET || (d) == PF_INET6)
#define ADDR_PORT(d, a) ((d) = ntohs(PF_INET ? IN_PORT(a) : IN6_PORT(a)))
#else
#define IS_DNS_DOMAIN(d) ((d) == AF_INET || (d) == AF_INET6)
#define ADDR_PORT(d, a) ((d) = ntohs(AF_INET ? IN_PORT(a) : IN6_PORT(a)))
#endif

static struct dns_sock_info *
lookup_sock_info(struct retrace_endpoint *ep, int fd)
{
	struct dns_info *di;
	struct dns_sock_info *si;

	di = &((struct handler_info *)ep->handle->user_data)->dns_info;
	SLIST_FOREACH(si, &di->sock_infos, next)
		if (si->pid == ep->pid || si->fd == fd)
			return si;

	return NULL;
}

static void
inherit(struct retrace_endpoint *ep)
{
	struct dns_info *di;
	struct dns_sock_infos new_infos;
	struct dns_sock_info *si, *new_si;

	di = &((struct handler_info *)ep->handle->user_data)->dns_info;
	SLIST_INIT(&new_infos);

	SLIST_FOREACH(si, &di->sock_infos, next) {
		if (si->pid != ep->ppid)
			continue;
		new_si = malloc(sizeof(struct dns_sock_info));
		if (new_si == NULL)
			break;
		memcpy(new_si, si, sizeof(struct dns_sock_info));
		new_si->pid = ep->pid;
		SLIST_INSERT_HEAD(&new_infos, new_si, next);
	}

	while (!SLIST_EMPTY(&new_infos)) {
		si = SLIST_FIRST(&new_infos);
		SLIST_REMOVE_HEAD(&new_infos, next);
		SLIST_INSERT_HEAD(&di->sock_infos, si, next);
	}
}

static void
dns_socket(struct retrace_endpoint *ep, struct retrace_call_context *ctx)
{
	struct dns_info *di;
	struct dns_sock_info *si;
	struct retrace_socket_params *params;
	int fd;

	fd = *(int *)ctx->result;
	if (fd == -1)
		return;

	params = (struct retrace_socket_params *)&ctx->params;
	if (!IS_DNS_DOMAIN(params->domain))
		return;

	si = malloc(sizeof(struct dns_sock_info));
	if (si == NULL)
		return;

	memset(si, 0, sizeof(struct dns_sock_info));
	si->fd = fd;
	si->pid = ep->pid;
	si->domain = params->domain;
	si->type = params->type;

	di = &((struct handler_info *)ep->handle->user_data)->dns_info;
	SLIST_INSERT_HEAD(&di->sock_infos, si, next);
}

static void
dns_close(struct retrace_endpoint *ep, struct retrace_call_context *ctx)
{
	struct dns_info *di;
	struct retrace_close_params *params;
	struct dns_sock_info *si;

	if (*(int *)ctx->result == -1)
		return;

	di = &((struct handler_info *)ep->handle->user_data)->dns_info;
	params = (struct retrace_close_params *)&ctx->params;

	si = lookup_sock_info(ep, params->fd);

	if (si != NULL) {
		SLIST_REMOVE(&di->sock_infos, si, dns_sock_info, next);
		free(si);
	}
}

static void
dns_bind(struct retrace_endpoint *ep, struct retrace_call_context *ctx)
{
	struct dns_info *di;
	struct retrace_bind_params *params;
	struct dns_sock_info *si;
	size_t len;

	if (*(int *)ctx->result == -1)
		return;

	di = &((struct handler_info *)ep->handle->user_data)->dns_info;
	params = (struct retrace_bind_params *)&ctx->params;
	si = lookup_sock_info(ep, params->sockfd);

	if (si == NULL)
		return;

	len = sizeof(si->addr);
	if (len > params->addrlen)
		len = params->addrlen;
	retrace_fetch_memory(ep->fd, params->addr, &si->addr, len);

	if (ADDR_PORT(si->domain, &si->addr) != 53) {
		SLIST_REMOVE(&di->sock_infos, si, dns_sock_info, next);
		free(si);
	}
}

static void
dns_recvfrom(struct retrace_endpoint *ep, struct retrace_call_context *ctx)
{
	struct retrace_recvfrom_params *params;
	struct dns_sock_info *si;
	ssize_t len;
	char *buf;
	struct sockaddr_storage addr;
	socklen_t addrlen;

	len = *(int *)ctx->result;
	if (len == -1)
		return;

	params = (struct retrace_recvfrom_params *)&ctx->params;
	si = lookup_sock_info(ep, params->sockfd);

	if (si == NULL)
		return;

	if (!retrace_fetch_memory(ep->fd, params->addrlen, &addrlen,
	    sizeof(addrlen)))
		return;

	if (addrlen > sizeof(addr))
		addrlen = sizeof(addr);

	if (!retrace_fetch_memory(ep->fd, params->src_addr, &addr, addrlen))
		return;

	buf = alloca(len);
	if (!retrace_fetch_memory(ep->fd, params->buf, buf, len))
		return;

	add_info(ctx, "DNS query of %d bytes received", len);
}

static void
dns_recv(struct retrace_endpoint *ep, struct retrace_call_context *ctx)
{
	if (*(int *)ctx->result == -1)
		return;
}

static void
dns_recvmsg(struct retrace_endpoint *ep, struct retrace_call_context *ctx)
{
	if (*(int *)ctx->result == -1)
		return;
}

void
init_dns_handlers(struct retrace_handle *handle)
{
	struct handler_info *hi = (struct handler_info *)handle->user_data;

	SLIST_INIT(&hi->dns_info.sock_infos);

	retrace_add_postcall_handler(handle, RPC_socket, dns_socket);
	retrace_add_postcall_handler(handle, RPC_bind, dns_bind);
	retrace_add_postcall_handler(handle, RPC_close, dns_close);
	retrace_add_postcall_handler(handle, RPC_recvfrom, dns_recvfrom);
	retrace_add_postcall_handler(handle, RPC_recv, dns_recv);
	retrace_add_postcall_handler(handle, RPC_recvmsg, dns_recvmsg);

	retrace_add_process_handler(handle, inherit);
}
