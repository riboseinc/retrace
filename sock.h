#ifndef __RETRACE_SOCK_H__
#define __RETRACE_SOCK_H__

#include <sys/types.h>

#ifndef __socklen_t_defined
typedef __socklen_t socklen_t;
#define __socklen_t_defined
#endif

#include <bits/sockaddr.h>

struct sockaddr {
    __SOCKADDR_COMMON(sa_);
    char sa_data[14];
};

#if ULONG_MAX > 0xffffffff
#define __ss_aligntype __uint64_t
#else
#define __ss_aligntype __uint32_t
#endif

struct msghdr {
    void *    msg_name;
    socklen_t msg_namelen;

    struct iovec *msg_iov;
    size_t        msg_iovlen;

    void * msg_control;
    size_t msg_controllen;

    int msg_flags;
};

extern struct cmsghdr *__cmsg_nxthdr(struct msghdr *__mhdr, struct cmsghdr *__cmsg) __THROW;

typedef int (*rtr_atoi_t)(const char *str);
typedef int (*rtr_bind_t)(int fd, const struct sockaddr *address, socklen_t len);
typedef int (*rtr_connect_t)(int fd, const struct sockaddr *address, socklen_t len);

rtr_atoi_t    real_atoi;
rtr_bind_t    real_bind;
rtr_connect_t real_connect;

#endif /* __RETRACE_SOCK_H__ */
