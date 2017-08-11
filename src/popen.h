#ifndef __RETRACE_POPEN_H__
#define __RETRACE_POPEN_H__

#include <stdio.h>

typedef FILE *(*rtr_popen_t)(const char *command, const char *type);
typedef int (*rtr_pclose_t)(FILE *stream);

RETRACE_DECL(popen);
RETRACE_DECL(pclose);

#endif /* __RETRACE_POPEN_H__ */
