#ifndef __SELECT_H__
#define __SELECT_H__

typedef int (*rtr_select_t)(int nfds, fd_set *readfds, fd_set *writefds,
                        fd_set *exceptfds, struct timeval *timeout);

rtr_select_t    real_select;

#endif /* __SELECT_H__ */
