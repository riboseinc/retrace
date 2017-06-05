#include "common.h"
#include "read.h"

ssize_t
read(int fd, void *buf, size_t nbytes)
{
    ssize_t ret;

    real_read = dlsym(RTLD_NEXT, "read");
    ret = real_read(fd, buf, nbytes);
    trace_printf(1, "read(%d, %p, %d); [%d]\n", fd, buf, nbytes, ret);

    return ret;
}
