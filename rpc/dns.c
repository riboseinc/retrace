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
#include "frontend.h"
#include "display.h"

static void
dns_recvfrom(struct retrace_endpoint *ep, struct retrace_call_context *ctx)
{
	if (*(int *)ctx->result == -1)
		return;
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
	retrace_add_postcall_handler(handle, RPC_recvfrom, dns_recvfrom);
	retrace_add_postcall_handler(handle, RPC_recv, dns_recv);
	retrace_add_postcall_handler(handle, RPC_recvmsg, dns_recvmsg);
}
