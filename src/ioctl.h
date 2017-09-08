#ifndef __RETRACE_IOCTL_H__
#define __RETRACE_IOCTL_H__


typedef int (*rtr_ioctl_t)(int fd, unsigned long request, ...);

RETRACE_DECL(ioctl);

#endif /* __RETRACE_IOCTL_H__ */
