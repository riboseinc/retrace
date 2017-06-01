#ifndef __RETRACE_TIME_H__
#define __RETRACE_TIME_H__

char *(*real_ctime)(const time_t *timep);
char *(*real_ctime_r)(const time_t *timep, char *buf);

#endif /* __RETRACE_TIME_H__ */
