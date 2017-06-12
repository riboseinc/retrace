#ifndef __RETRACE_SSL_H__
#define __RETRACE_SSL_H__

#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>


typedef int (*rtr_SSL_write_t)(SSL *ssl, const void *buf, int num);
typedef int (*rtr_SSL_read_t)(SSL *ssl, void *buf, int num);
typedef int (*rtr_SSL_accept_t)(SSL *ssl);
typedef int (*rtr_SSL_connect_t)(SSL *ssl);


RETRACE_DECL(SSL_write);
RETRACE_DECL(SSL_read);
RETRACE_DECL(SSL_accept);
RETRACE_DECL(SSL_connect);



#endif /* __RETRACE_SSL_H__ */

