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

typedef ssize_t (*rtr_send_t)(int sockfd, const void *buf, size_t len, int flags);
typedef ssize_t (*rtr_sendto_t)(int sockfd, const void *buf, size_t len, int flags,
		const struct sockaddr *dest_addr, socklen_t addrlen);
typedef ssize_t (*rtr_sendmsg_t)(int sockfd, const struct msghdr *msg, int flags);

typedef ssize_t (*rtr_recv_t)(int sockfd, void *buf, size_t len, int flags);
typedef ssize_t (*rtr_recvfrom_t)(int sockfd, void *buf, size_t len, int flags,
	struct sockaddr *src_addr, socklen_t *addrlen);
typedef ssize_t (*rtr_recvmsg_t)(int sockfd, struct msghdr *msg, int flags);

RETRACE_DECL(socket);
RETRACE_DECL(connect);
RETRACE_DECL(bind);
RETRACE_DECL(accept);

RETRACE_DECL(setsockopt);

RETRACE_DECL(send);
RETRACE_DECL(sendto);
RETRACE_DECL(sendmsg);

RETRACE_DECL(recv);
RETRACE_DECL(recvfrom);
RETRACE_DECL(recvmsg);

#endif /* __RETRACE_SOCK_H__ */
