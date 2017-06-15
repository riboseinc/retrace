#ifndef __RETRACE_PLEDGE_H__
#define __RETRACE_PLEDGE_H__

#ifdef __OpenBSD__

typedef int (*rtr_pledge_t)(const char *promises, const char *paths[]); 

RETRACE_DECL(pledge);

#endif /* __OpenBSD */

#endif /* __RETRACE_PLEDGE_H__ */
