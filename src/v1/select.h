#ifndef __RETRACE_SELECT_H__
#define __RETRACE_SELECT_H__

typedef int (*rtr_select_t)(int nfds, fd_set *readfds, fd_set *writefds,
			    fd_set *exceptfds, struct timeval *timeout);

RETRACE_DECL(select);

#endif /* __RETRACE_SELECT_H__ */
