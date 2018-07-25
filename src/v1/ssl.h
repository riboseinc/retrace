#ifndef __RETRACE_SSL_H__
#define __RETRACE_SSL_H__

#include "config.h"

#ifdef HAVE_OPENSSL_SSL_H
#include <unistd.h>
#include <sys/types.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>


typedef int (*rtr_SSL_write_t)(SSL *ssl, const void *buf, int num);
typedef int (*rtr_SSL_read_t)(SSL *ssl, void *buf, int num);
typedef int (*rtr_SSL_accept_t)(SSL *ssl);
typedef int (*rtr_SSL_connect_t)(SSL *ssl);
typedef long (*rtr_SSL_get_verify_result_t)(const SSL *ssl);
typedef long (*rtr_BIO_ctrl_t)(BIO *bp, int cmd, long larg, void *parg);

/* No replacing these */
typedef size_t (*rtr_SSL_get_client_random_t)(const SSL *ssl, unsigned char *out, size_t outlen);
typedef size_t (*rtr_SSL_get_server_random_t)(const SSL *ssl, unsigned char *out, size_t outlen);
typedef size_t (*rtr_SSL_SESSION_get_master_key_t)(const SSL_SESSION *session, unsigned char *out, size_t outlen);
typedef SSL_SESSION *(*rtr_SSL_get_session_t)(const SSL *ssl);



RETRACE_DECL(SSL_write);
RETRACE_DECL(SSL_read);
RETRACE_DECL(SSL_accept);
RETRACE_DECL(SSL_connect);
RETRACE_DECL(SSL_get_verify_result);
RETRACE_DECL(BIO_ctrl);

#endif /* HAVE_OPENSSL_SSL_H */

#endif /* __RETRACE_SSL_H__ */

