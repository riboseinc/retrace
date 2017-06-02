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

rtr_getgid_t	real_getgid;
rtr_getegid_t	real_getegid;
rtr_getuid_t	real_getuid;
rtr_geteuid_t	real_geteuid;
rtr_setuid_t	real_setuid;
rtr_seteuid_t	real_seteuid;
rtr_setgid_t	real_setgid;
rtr_getpid_t	real_getpid;
rtr_getppid_t	real_getppid;

#endif /* __RETRACE_ID_H__ */
