#ifndef __RETRACE_TIME_H__
#define __RETRACE_TIME_H__

#include <time.h>

typedef char *(*rtr_ctime_t)(const time_t *timep);
typedef char *(*rtr_ctime_r_t)(const time_t *timep, char *buf);
typedef int (*rtr_gettimeofday_t)(struct timeval *tv, struct timezone *tz);

rtr_ctime_t   real_ctime;
rtr_ctime_r_t real_ctime_r;
rtr_gettimeofday_t real_gettimeofday;

#endif /* __RETRACE_TIME_H__ */
