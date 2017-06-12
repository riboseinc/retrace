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

#include <openssl/ssl.h>

#include "common.h"
#include "ssl.h"

static void 
print_key(const unsigned char *buf, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		trace_printf(0, "%02X", buf[i]);
	}
}

static void
print_ssl_keys(SSL *ssl)
{
	if (ssl &&
	    ssl->s3 != NULL &&
	    ssl->session != NULL &&
	    ssl->session->master_key_length > 0 ) {
		trace_printf(0, "\tCLIENT_RANDOM ");
		print_key(ssl->s3->client_random, SSL3_RANDOM_SIZE);
		trace_printf(0, " ");
		print_key(ssl->session->master_key, ssl->session->master_key_length);
		trace_printf(0, "\n");
	}
}


int RETRACE_IMPLEMENTATION(SSL_write)(SSL *ssl, const void *buf, int num)
{
	rtr_SSL_write_t real_SSL_write;
	int r;

	real_SSL_write = RETRACE_GET_REAL(SSL_write);

	r = real_SSL_write(ssl, buf, num);

	trace_printf(1, "SSL_write(%p, %p, %d); [return: %d]\n", ssl, buf, num, r);
	trace_dump_data(buf, num);

	return (r);
}

RETRACE_REPLACE(SSL_write)

int RETRACE_IMPLEMENTATION(SSL_read)(SSL *ssl, void *buf, int num)
{
	rtr_SSL_read_t real_SSL_read;
	int r;

	real_SSL_read = RETRACE_GET_REAL(SSL_read);

	r = real_SSL_read(ssl, buf, num);

	trace_printf(1, "SSL_read(%p, %p, %d); [return: %d]\n", ssl, buf, num, r);

	if (r > 0)
		trace_dump_data(buf, r);

	return (r);
}

RETRACE_REPLACE(SSL_read)

int RETRACE_IMPLEMENTATION(SSL_connect)(SSL *ssl)
{
	rtr_SSL_connect_t real_SSL_connect;
	int r;

	real_SSL_connect = RETRACE_GET_REAL(SSL_connect);

	r = real_SSL_connect(ssl);

	trace_printf(1, "SSL_connect(%p); [return: %d]\n", ssl, r);
	print_ssl_keys (ssl);

	return (r);
}

RETRACE_REPLACE(SSL_connect)

int RETRACE_IMPLEMENTATION(SSL_accept)(SSL *ssl)
{
	rtr_SSL_accept_t real_SSL_accept;
	int r;

	real_SSL_accept = RETRACE_GET_REAL(SSL_accept);

	r = real_SSL_accept(ssl);

	trace_printf(1, "SSL_accept(%p); [return: %d]\n", ssl, r);
	print_ssl_keys(ssl);

	return (r);
}

RETRACE_REPLACE(SSL_accept)

