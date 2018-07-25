#ifndef __RETRACE_ID_H__
#define __RETRACE_ID_H__

typedef gid_t (*rtr_getgid_t)();
typedef gid_t (*rtr_getegid_t)();
typedef uid_t (*rtr_getuid_t)();
typedef uid_t (*rtr_geteuid_t)();
typedef int (*rtr_setuid_t)(uid_t uid);
typedef int (*rtr_seteuid_t)(uid_t uid);
typedef int (*rtr_setgid_t)(gid_t gid);
typedef pid_t (*rtr_getpid_t)(void);
typedef pid_t (*rtr_getppid_t)(void);

RETRACE_DECL(getgid);
RETRACE_DECL(getegid);
RETRACE_DECL(getuid);
RETRACE_DECL(geteuid);
RETRACE_DECL(setuid);
RETRACE_DECL(seteuid);
RETRACE_DECL(setgid);
RETRACE_DECL(getpid);
RETRACE_DECL(getppid);

#endif /* __RETRACE_ID_H__ */
