#ifndef __RETRACE_ID_H__
#define __RETRACE_ID_H__

gid_t (*real_getgid)();
gid_t (*real_getegid)();
uid_t (*real_getuid)();
uid_t (*real_geteuid)();
int (*real_setuid)(uid_t uid);
int (*real_seteuid)(uid_t uid);
int (*real_setgid)(gid_t gid);
pid_t (*real_getpid)(void);
pid_t (*real_getppid)(void);

#endif /* __RETRACE_ID_H__ */
