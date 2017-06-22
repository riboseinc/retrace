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
#include <openssl/ssl.h>
#include <openssl/bio.h>

#include "ssl.h"


typedef size_t (*rtr_SSL_get_client_random_t)(const SSL *ssl, unsigned char *out, size_t outlen);
typedef size_t (*rtr_SSL_get_server_random_t)(const SSL *ssl, unsigned char *out, size_t outlen);
typedef size_t (*rtr_SSL_SESSION_get_master_key_t)(const SSL_SESSION *session, unsigned char *out, size_t outlen);
typedef SSL_SESSION *(*rtr_SSL_get_session_t)(const SSL *ssl);


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
	SSL_SESSION *session = NULL;
	size_t master_key_length = 0;
	size_t client_random_length = 0;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	unsigned char *client_random;
	unsigned char *master_key;
#else
	unsigned char client_random[SSL3_RANDOM_SIZE];
	unsigned char master_key[SSL_MAX_MASTER_KEY_LENGTH];
#endif

	if (ssl) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
		session = ssl->session;

		if (ssl->s3) {
			client_random = ssl->s3->client_random;
			client_random_length = SSL3_RANDOM_SIZE;
		}

		if (session) {
			master_key = session->master_key;
			master_key_length = session->master_key_length;
		}
#else
		rtr_SSL_get_session_t real_SSL_get_session;
		rtr_SSL_SESSION_get_master_key_t real_SSL_SESSION_get_master_key;
		rtr_SSL_get_client_random_t real_SSL_get_client_random;

		*(void **) &real_SSL_get_client_random = dlsym(RTLD_DEFAULT, "SSL_get_client_random");
		*(void **) &real_SSL_SESSION_get_master_key = dlsym(RTLD_DEFAULT, "SSL_SESSION_get_master_key");
		*(void **) &real_SSL_get_session = dlsym(RTLD_DEFAULT, "SSL_get_session");

		if (real_SSL_get_client_random &&
		    real_SSL_SESSION_get_master_key &&
		    real_SSL_get_client_random) {
			session = real_SSL_get_session (ssl);

			if (session) {
				master_key_length = real_SSL_SESSION_get_master_key(session, master_key, SSL_MAX_MASTER_KEY_LENGTH);
				client_random_length = real_SSL_get_client_random(ssl, client_random, SSL3_RANDOM_SIZE);
			}
		}
#endif
	}

	if (master_key_length > 0 && client_random_length > 0) {
		trace_printf(0, "\tCLIENT_RANDOM ");
		print_key(client_random, client_random_length);
		trace_printf(0, " ");
		print_key(master_key, master_key_length);
		trace_printf(0, "\n");
	}
}

int RETRACE_IMPLEMENTATION(SSL_write)(SSL *ssl, const void *buf, int num)
{
	int r;

	r = real_SSL_write(ssl, buf, num);

	trace_printf(1, "SSL_write(%p, %p, %d); [return: %d]\n", ssl, buf, num, r);
	trace_dump_data(buf, num);

	return (r);
}

RETRACE_REPLACE(SSL_write, int, (SSL * ssl, const void *buf, int num),
	(ssl, buf, num))


int RETRACE_IMPLEMENTATION(SSL_read)(SSL *ssl, void *buf, int num)
{
	int r;

	r = real_SSL_read(ssl, buf, num);

	trace_printf(1, "SSL_read(%p, %p, %d); [return: %d]\n", ssl, buf, num, r);

	if (r > 0)
		trace_dump_data(buf, r);

	return (r);
}

RETRACE_REPLACE(SSL_read, int, (SSL * ssl, void *buf, int num),
	(ssl, buf, num))


int RETRACE_IMPLEMENTATION(SSL_connect)(SSL *ssl)
{
	int r;

	r = real_SSL_connect(ssl);

	trace_printf(1, "SSL_connect(%p); [return: %d]\n", ssl, r);
	print_ssl_keys (ssl);

	return (r);
}

RETRACE_REPLACE(SSL_connect, int, (SSL * ssl), (ssl))



int RETRACE_IMPLEMENTATION(SSL_accept)(SSL *ssl)
{
	int r;

	r = real_SSL_accept(ssl);

	trace_printf(1, "SSL_accept(%p); [return: %d]\n", ssl, r);
	print_ssl_keys(ssl);

	return (r);
}

RETRACE_REPLACE(SSL_accept, int, (SSL * ssl), (ssl))


long
RETRACE_IMPLEMENTATION(SSL_get_verify_result)(const SSL *ssl)
{
	int r;
	int redirect_id = 0;

	if (rtr_get_config_single("SSL_get_verify_result", ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END, &redirect_id)) {

		r = real_SSL_get_verify_result(ssl);

		trace_printf(1, "SSL_get_verify_result(%p); [redirection in effect: '%i']\n", ssl, redirect_id);

		return redirect_id;
	}

	r = real_SSL_get_verify_result(ssl);

	trace_printf(1, "SSL_get_verify_result(%p); [return: %d]\n", ssl, r);

	return r;
}

RETRACE_REPLACE(SSL_get_verify_result, long, (const SSL * ssl), (ssl))


#define DEFINE_TO_STR(def, str) case (def): str = #def; break;

long RETRACE_IMPLEMENTATION(BIO_ctrl)(BIO *bp, int cmd, long larg, void *parg)
{
	long r;
	SSL *ssl = NULL;
	char *cmd_str;

	r = real_BIO_ctrl(bp, cmd, larg, parg);

	switch (cmd) {
#ifdef BIO_CTRL_RESET
	DEFINE_TO_STR(BIO_CTRL_RESET, cmd_str)
#endif
#ifdef BIO_CTRL_EOF
	DEFINE_TO_STR(BIO_CTRL_EOF, cmd_str)
#endif
#ifdef BIO_CTRL_INFO
	DEFINE_TO_STR(BIO_CTRL_INFO, cmd_str)
#endif
#ifdef BIO_CTRL_SET
	DEFINE_TO_STR(BIO_CTRL_SET, cmd_str)
#endif
#ifdef BIO_CTRL_GET
	DEFINE_TO_STR(BIO_CTRL_GET, cmd_str)
#endif
#ifdef BIO_CTRL_PUSH
	DEFINE_TO_STR(BIO_CTRL_PUSH, cmd_str)
#endif
#ifdef BIO_CTRL_POP
	DEFINE_TO_STR(BIO_CTRL_POP, cmd_str)
#endif
#ifdef BIO_CTRL_GET_CLOSE
	DEFINE_TO_STR(BIO_CTRL_GET_CLOSE, cmd_str)
#endif
#ifdef BIO_CTRL_SET_CLOSE
	DEFINE_TO_STR(BIO_CTRL_SET_CLOSE, cmd_str)
#endif
#ifdef BIO_CTRL_PENDING
	DEFINE_TO_STR(BIO_CTRL_PENDING, cmd_str)
#endif
#ifdef BIO_CTRL_FLUSH
	DEFINE_TO_STR(BIO_CTRL_FLUSH, cmd_str)
#endif
#ifdef BIO_CTRL_DUP
	DEFINE_TO_STR(BIO_CTRL_DUP, cmd_str)
#endif
#ifdef BIO_CTRL_WPENDING
	DEFINE_TO_STR(BIO_CTRL_WPENDING, cmd_str)
#endif
#ifdef BIO_CTRL_SET_CALLBACK
	DEFINE_TO_STR(BIO_CTRL_SET_CALLBACK, cmd_str)
#endif
#ifdef BIO_CTRL_GET_CALLBACK
	DEFINE_TO_STR(BIO_CTRL_GET_CALLBACK, cmd_str)
#endif
#ifdef BIO_CTRL_SET_FILENAME
	DEFINE_TO_STR(BIO_CTRL_SET_FILENAME, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_CONNECT
	DEFINE_TO_STR(BIO_CTRL_DGRAM_CONNECT, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SET_CONNECTED
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SET_CONNECTED, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SET_RECV_TIMEOUT
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_GET_RECV_TIMEOUT
	DEFINE_TO_STR(BIO_CTRL_DGRAM_GET_RECV_TIMEOUT, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SET_SEND_TIMEOUT
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_GET_SEND_TIMEOUT
	DEFINE_TO_STR(BIO_CTRL_DGRAM_GET_SEND_TIMEOUT, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_GET_RECV_TIMER_EXP
	DEFINE_TO_STR(BIO_CTRL_DGRAM_GET_RECV_TIMER_EXP, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_GET_SEND_TIMER_EXP
	DEFINE_TO_STR(BIO_CTRL_DGRAM_GET_SEND_TIMER_EXP, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_MTU_DISCOVER
	DEFINE_TO_STR(BIO_CTRL_DGRAM_MTU_DISCOVER, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_QUERY_MTU
	DEFINE_TO_STR(BIO_CTRL_DGRAM_QUERY_MTU, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_GET_FALLBACK_MTU
	DEFINE_TO_STR(BIO_CTRL_DGRAM_GET_FALLBACK_MTU, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_GET_MTU
	DEFINE_TO_STR(BIO_CTRL_DGRAM_GET_MTU, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SET_MTU
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SET_MTU, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_MTU_EXCEEDED
	DEFINE_TO_STR(BIO_CTRL_DGRAM_MTU_EXCEEDED, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_GET_PEER
	DEFINE_TO_STR(BIO_CTRL_DGRAM_GET_PEER, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SET_PEER
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SET_PEER, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SET_DONT_FRAG
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SET_DONT_FRAG, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_GET_MTU_OVERHEAD
	DEFINE_TO_STR(BIO_CTRL_DGRAM_GET_MTU_OVERHEAD, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SET_PEEK_MODE
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SET_PEEK_MODE, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SCTP_SET_IN_HANDSHAKE
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SCTP_SET_IN_HANDSHAKE, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SCTP_ADD_AUTH_KEY
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SCTP_ADD_AUTH_KEY, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SCTP_NEXT_AUTH_KEY
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SCTP_NEXT_AUTH_KEY, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SCTP_AUTH_CCS_RCVD
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SCTP_AUTH_CCS_RCVD, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SCTP_GET_SNDINFO
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SCTP_GET_SNDINFO, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SCTP_SET_SNDINFO
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SCTP_SET_SNDINFO, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SCTP_GET_RCVINFO
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SCTP_GET_RCVINFO, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SCTP_SET_RCVINFO
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SCTP_SET_RCVINFO, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SCTP_GET_PRINFO
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SCTP_GET_PRINFO, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SCTP_SET_PRINFO
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SCTP_SET_PRINFO, cmd_str)
#endif
#ifdef BIO_CTRL_DGRAM_SCTP_SAVE_SHUTDOWN
	DEFINE_TO_STR(BIO_CTRL_DGRAM_SCTP_SAVE_SHUTDOWN, cmd_str)
#endif
#ifdef BIO_C_SET_CONNECT
	DEFINE_TO_STR(BIO_C_SET_CONNECT, cmd_str)
#endif
#ifdef BIO_C_DO_STATE_MACHINE
	DEFINE_TO_STR(BIO_C_DO_STATE_MACHINE, cmd_str)
#endif
#ifdef BIO_C_SET_NBIO
	DEFINE_TO_STR(BIO_C_SET_NBIO, cmd_str)
#endif
#ifdef BIO_C_SET_FD
	DEFINE_TO_STR(BIO_C_SET_FD, cmd_str)
#endif
#ifdef BIO_C_GET_FD
	DEFINE_TO_STR(BIO_C_GET_FD, cmd_str)
#endif
#ifdef BIO_C_SET_FILE_PTR
	DEFINE_TO_STR(BIO_C_SET_FILE_PTR, cmd_str)
#endif
#ifdef BIO_C_GET_FILE_PTR
	DEFINE_TO_STR(BIO_C_GET_FILE_PTR, cmd_str)
#endif
#ifdef BIO_C_SET_FILENAME
	DEFINE_TO_STR(BIO_C_SET_FILENAME, cmd_str)
#endif
#ifdef BIO_C_SET_SSL
	DEFINE_TO_STR(BIO_C_SET_SSL, cmd_str)
#endif
#ifdef BIO_C_GET_SSL
	DEFINE_TO_STR(BIO_C_GET_SSL, cmd_str)
#endif
#ifdef BIO_C_SET_MD
	DEFINE_TO_STR(BIO_C_SET_MD, cmd_str)
#endif
#ifdef BIO_C_GET_MD
	DEFINE_TO_STR(BIO_C_GET_MD, cmd_str)
#endif
#ifdef BIO_C_GET_CIPHER_STATUS
	DEFINE_TO_STR(BIO_C_GET_CIPHER_STATUS, cmd_str)
#endif
#ifdef BIO_C_SET_BUF_MEM
	DEFINE_TO_STR(BIO_C_SET_BUF_MEM, cmd_str)
#endif
#ifdef BIO_C_GET_BUF_MEM_PTR
	DEFINE_TO_STR(BIO_C_GET_BUF_MEM_PTR, cmd_str)
#endif
#ifdef BIO_C_GET_BUFF_NUM_LINES
	DEFINE_TO_STR(BIO_C_GET_BUFF_NUM_LINES, cmd_str)
#endif
#ifdef BIO_C_SET_BUFF_SIZE
	DEFINE_TO_STR(BIO_C_SET_BUFF_SIZE, cmd_str)
#endif
#ifdef BIO_C_SET_ACCEPT
	DEFINE_TO_STR(BIO_C_SET_ACCEPT, cmd_str)
#endif
#ifdef BIO_C_SSL_MODE
	DEFINE_TO_STR(BIO_C_SSL_MODE, cmd_str)
#endif
#ifdef BIO_C_GET_MD_CTX
	DEFINE_TO_STR(BIO_C_GET_MD_CTX, cmd_str)
#endif
#ifdef BIO_C_SET_BUFF_READ_DATA
	DEFINE_TO_STR(BIO_C_SET_BUFF_READ_DATA, cmd_str)
#endif
#ifdef BIO_C_GET_CONNECT
	DEFINE_TO_STR(BIO_C_GET_CONNECT, cmd_str)
#endif
#ifdef BIO_C_GET_ACCEPT
	DEFINE_TO_STR(BIO_C_GET_ACCEPT, cmd_str)
#endif
#ifdef BIO_C_SET_SSL_RENEGOTIATE_BYTES
	DEFINE_TO_STR(BIO_C_SET_SSL_RENEGOTIATE_BYTES, cmd_str)
#endif
#ifdef BIO_C_GET_SSL_NUM_RENEGOTIATES
	DEFINE_TO_STR(BIO_C_GET_SSL_NUM_RENEGOTIATES, cmd_str)
#endif
#ifdef BIO_C_SET_SSL_RENEGOTIATE_TIMEOUT
	DEFINE_TO_STR(BIO_C_SET_SSL_RENEGOTIATE_TIMEOUT, cmd_str)
#endif
#ifdef BIO_C_FILE_SEEK
	DEFINE_TO_STR(BIO_C_FILE_SEEK, cmd_str)
#endif
#ifdef BIO_C_GET_CIPHER_CTX
	DEFINE_TO_STR(BIO_C_GET_CIPHER_CTX, cmd_str)
#endif
#ifdef BIO_C_SET_BUF_MEM_EOF_RETURN
	DEFINE_TO_STR(BIO_C_SET_BUF_MEM_EOF_RETURN, cmd_str)
#endif
#ifdef BIO_C_SET_BIND_MODE
	DEFINE_TO_STR(BIO_C_SET_BIND_MODE, cmd_str)
#endif
#ifdef BIO_C_GET_BIND_MODE
	DEFINE_TO_STR(BIO_C_GET_BIND_MODE, cmd_str)
#endif
#ifdef BIO_C_FILE_TELL
	DEFINE_TO_STR(BIO_C_FILE_TELL, cmd_str)
#endif
#ifdef BIO_C_GET_SOCKS
	DEFINE_TO_STR(BIO_C_GET_SOCKS, cmd_str)
#endif
#ifdef BIO_C_SET_SOCKS
	DEFINE_TO_STR(BIO_C_SET_SOCKS, cmd_str)
#endif
#ifdef BIO_C_SET_WRITE_BUF_SIZE
	DEFINE_TO_STR(BIO_C_SET_WRITE_BUF_SIZE, cmd_str)
#endif
#ifdef BIO_C_GET_WRITE_BUF_SIZE
	DEFINE_TO_STR(BIO_C_GET_WRITE_BUF_SIZE, cmd_str)
#endif
#ifdef BIO_C_MAKE_BIO_PAIR
	DEFINE_TO_STR(BIO_C_MAKE_BIO_PAIR, cmd_str)
#endif
#ifdef BIO_C_DESTROY_BIO_PAIR
	DEFINE_TO_STR(BIO_C_DESTROY_BIO_PAIR, cmd_str)
#endif
#ifdef BIO_C_GET_WRITE_GUARANTEE
	DEFINE_TO_STR(BIO_C_GET_WRITE_GUARANTEE, cmd_str)
#endif
#ifdef BIO_C_GET_READ_REQUEST
	DEFINE_TO_STR(BIO_C_GET_READ_REQUEST, cmd_str)
#endif
#ifdef BIO_C_SHUTDOWN_WR
	DEFINE_TO_STR(BIO_C_SHUTDOWN_WR, cmd_str)
#endif
#ifdef BIO_C_NREAD0
	DEFINE_TO_STR(BIO_C_NREAD0, cmd_str)
#endif
#ifdef BIO_C_NREAD
	DEFINE_TO_STR(BIO_C_NREAD, cmd_str)
#endif
#ifdef BIO_C_NWRITE0
	DEFINE_TO_STR(BIO_C_NWRITE0, cmd_str)
#endif
#ifdef BIO_C_NWRITE
	DEFINE_TO_STR(BIO_C_NWRITE, cmd_str)
#endif
#ifdef BIO_C_RESET_READ_REQUEST
	DEFINE_TO_STR(BIO_C_RESET_READ_REQUEST, cmd_str)
#endif
#ifdef BIO_C_SET_MD_CTX
	DEFINE_TO_STR(BIO_C_SET_MD_CTX, cmd_str)
#endif
#ifdef BIO_C_SET_PREFIX
	DEFINE_TO_STR(BIO_C_SET_PREFIX, cmd_str)
#endif
#ifdef BIO_C_GET_PREFIX
	DEFINE_TO_STR(BIO_C_GET_PREFIX, cmd_str)
#endif
#ifdef BIO_C_SET_SUFFIX
	DEFINE_TO_STR(BIO_C_SET_SUFFIX, cmd_str)
#endif
#ifdef BIO_C_GET_SUFFIX
	DEFINE_TO_STR(BIO_C_GET_SUFFIX, cmd_str)
#endif
#ifdef BIO_C_SET_EX_ARG
	DEFINE_TO_STR(BIO_C_SET_EX_ARG, cmd_str)
#endif
#ifdef BIO_C_GET_EX_ARG
	DEFINE_TO_STR(BIO_C_GET_EX_ARG, cmd_str)
#endif
#ifdef BIO_C_SET_CONNECT_MODE
	DEFINE_TO_STR(BIO_C_SET_CONNECT_MODE, cmd_str)
#endif

	default:
		cmd_str = "UNKNOWN";
	}

	trace_printf(1, "BIO_ctrl(%p, \"%s\"(%d), %ld, %p); [return: %d]\n", bp, cmd_str, cmd, larg, parg, r);

	if (cmd == BIO_C_DO_STATE_MACHINE) {
		if (get_tracing_enabled()) {
			int old_trace_state;

			old_trace_state = trace_disable();
			BIO_get_ssl(bp, &ssl);
			trace_restore(old_trace_state);
		}

		if (ssl)
			print_ssl_keys(ssl);
	}

	return (r);
}

RETRACE_REPLACE(BIO_ctrl, long, (BIO * bp, int cmd, long larg, void *parg),
	(bp, cmd, larg, parg))
