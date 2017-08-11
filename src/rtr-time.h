#ifndef __RETRACE_TIME_H__
#define __RETRACE_TIME_H__

#include <time.h>

typedef char *(*rtr_ctime_t)(const time_t *timep);
typedef char *(*rtr_ctime_r_t)(const time_t *timep, char *buf);

#ifdef __APPLE__
typedef int (*rtr_gettimeofday_t)(struct timeval *restrict tv, void *restrict tzp);
#else
typedef int (*rtr_gettimeofday_t)(struct timeval *tv, struct timezone *tz);
#endif

RETRACE_DECL(ctime);
RETRACE_DECL(ctime_r);
RETRACE_DECL(gettimeofday);

#endif /* __RETRACE_TIME_H__ */
