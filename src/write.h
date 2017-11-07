#ifndef __RETRACE_WRITE_H__
#define __RETRACE_WRITE_H__

#include <unistd.h>

typedef ssize_t (*rtr_write_t)(int fd, const void *buf, size_t nbytes);
typedef ssize_t (*rtr_writev_t)(int fd, const struct iovec *iov, int iovcnt);

RETRACE_DECL(write);
RETRACE_DECL(writev);

#endif /* __RETRACE_WRITE_H__ */
