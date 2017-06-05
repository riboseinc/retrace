#ifndef __RETRACE_WRITE_H__
#define __RETRACE_WRITE_H__

#include <unistd.h>

typedef ssize_t (*rtr_write_t)(int fd, const void *buf, size_t nbytes);

rtr_write_t real_write;

#endif /* __RETRACE_WRITE_H__ */
