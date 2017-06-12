#ifndef __RETRACE_SOCK_H__
#define __RETRACE_SOCK_H__

#include <sys/types.h>
#include <sys/socket.h>

#ifndef AF_INET
#define AF_INET 2
#endif

typedef int (*rtr_atoi_t)(const char *str);
typedef int (*rtr_accept_t)(int fd, struct sockaddr *address, socklen_t *len);
typedef int (*rtr_bind_t)(int fd, const struct sockaddr *address, socklen_t len);
typedef int (*rtr_connect_t)(int fd, const struct sockaddr *address, socklen_t len);

RETRACE_DECL(atoi);
RETRACE_DECL(accept);
RETRACE_DECL(bind);
RETRACE_DECL(connect);

#endif /* __RETRACE_SOCK_H__ */
