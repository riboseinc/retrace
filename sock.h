#ifndef __RETRACE_SOCK_H__
#define __RETRACE_SOCK_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

typedef int (*rtr_socket_t)(int domain, int type, int protocol);
typedef int (*rtr_connect_t)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
typedef int (*rtr_bind_t)(int fd, const struct sockaddr *address, socklen_t len);
typedef int (*rtr_accept_t)(int fd, struct sockaddr *address, socklen_t *len);

typedef int (*rtr_setsockopt_t)(int fd, int level, int optname, const void *optval, socklen_t optlen);

RETRACE_DECL(socket);
RETRACE_DECL(connect);
RETRACE_DECL(bind);
RETRACE_DECL(accept);

RETRACE_DECL(setsockopt);

#endif /* __RETRACE_SOCK_H__ */
