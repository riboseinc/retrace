#ifndef __RETRACE_SSL_H__
#define __RETRACE_SSL_H__

#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>


typedef int (*rtr_SSL_write_t)(SSL *ssl, const void *buf, int num);
typedef int (*rtr_SSL_read_t)(SSL *ssl, void *buf, int num);
typedef int (*rtr_SSL_accept_t)(SSL *ssl);
typedef int (*rtr_SSL_connect_t)(SSL *ssl);
typedef long (*rtr_SSL_get_verify_result_t)(const SSL *ssl);
typedef long (*rtr_BIO_ctrl_t)(BIO *bp, int cmd, long larg, void *parg);


RETRACE_DECL(SSL_write);
RETRACE_DECL(SSL_read);
RETRACE_DECL(SSL_accept);
RETRACE_DECL(SSL_connect);
RETRACE_DECL(SSL_get_verify_result);
RETRACE_DECL(BIO_ctrl);

#endif /* __RETRACE_SSL_H__ */

