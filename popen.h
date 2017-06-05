#ifndef __RETRACE_POPEN_H__
#define __RETRACE_POPEN_H__

#include <stdio.h>

typedef FILE *(*rtr_popen_t)(const char *command, const char *type);
typedef int (*rtr_pclose_t)(FILE *stream);

rtr_popen_t  real_popen;
rtr_pclose_t real_pclose;

#endif /* __RETRACE_POPEN_H__ */
