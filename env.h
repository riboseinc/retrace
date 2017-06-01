#ifndef __RETRACE_ENV_H__
#define __RETRACE_ENV_H__

static char *(*real_getenv)(const char *name);
static int (*real_putenv)(char *string);
static int (*real_unsetenv)(const char *name);

#endif /* __RETRACE_ENV_H__ */
