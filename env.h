#ifndef __RETRACE_ENV_H__
#define __RETRACE_ENV_H__

typedef char *(*rtr_getenv_t)(const char *name);
typedef int (*rtr_putenv_t)(char *string);
typedef int (*rtr_unsetenv_t)(const char *name);

typedef int (*rtr_uname_t)(struct utsname *buf);

rtr_getenv_t   real_getenv;
rtr_putenv_t   real_putenv;
rtr_unsetenv_t real_unsetenv;

rtr_uname_t    real_uname;

#endif /* __RETRACE_ENV_H__ */
