#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include "sock.h"

#define RTR_MAX_PATH                            128
#define RTR_MAX_PLUGIN_NAME_LEN                 128

// retrace plugin types
enum RTR_PLUGIN_TYPE
{
        RTR_PLUGIN_TYPE_SOCK,
        RTR_PLUGIN_TYPE_MAX,
};

// plugin hooking functions
typedef struct rtr_plugin_sock
{
        int (*p_connect)(int fd, const struct sockaddr *address, socklen_t len);

        ssize_t (*p_send)(int fd, const void *buf, size_t len, int flags);
        ssize_t (*p_recv)(int fd, void *buf, size_t len, int flags);

        ssize_t (*p_write)(int fd, const void *buf, size_t count);
        ssize_t (*p_read)(int fd, void *buf, size_t count);
} rtr_plugin_sock_t;

// retrace plugin functions
void *rtr_plugin_get(int plugin_type);

#endif /* __PLUGIN_H__ */
