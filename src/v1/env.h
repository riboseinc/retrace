#ifndef __RETRACE_ENV_H__
#define __RETRACE_ENV_H__

typedef char *(*rtr_getenv_t)(const char *name);
typedef int (*rtr_putenv_t)(char *string);
typedef int (*rtr_unsetenv_t)(const char *name);

typedef int (*rtr_uname_t)(struct utsname *buf);

RETRACE_DECL(getenv);
RETRACE_DECL(putenv);
RETRACE_DECL(unsetenv);
RETRACE_DECL(uname);
#endif /* __RETRACE_ENV_H__ */
