#ifndef __RETRACE_SOCK_H__
#define __RETRACE_SOCK_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef AF_INET
#define AF_INET 2
#endif

typedef int (*rtr_atoi_t)(const char *str);
typedef int (*rtr_accept_t)(int fd, struct sockaddr *address, socklen_t *len);
typedef int (*rtr_bind_t)(int fd, const struct sockaddr *address, socklen_t len);
typedef int (*rtr_connect_t)(int fd, const struct sockaddr *address, socklen_t len);
typedef ssize_t (*rtr_send_t)(int fd, const void *buf, size_t len, int flags);
typedef ssize_t (*rtr_recv_t)(int fd, void *buf, size_t len, int flags);

rtr_atoi_t    real_atoi;
rtr_accept_t  real_accept;
rtr_bind_t    real_bind;
rtr_connect_t real_connect;
rtr_send_t    real_send;
rtr_recv_t    real_recv;

#endif /* __RETRACE_SOCK_H__ */
