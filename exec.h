#ifndef __RETRACE_EXEC_H__
#define __RETRACE_EXEC_H__

typedef int (*rtr_execl_t)(const char *path, const char *arg0, ... /*, (char *)0 */);
typedef int (*rtr_execvp_t)(const char *path, char *const argv[]);
typedef int (*rtr_execve_t)(const char *path, char *const argv[], char *const envp[]);
typedef int (*rtr_execle_t)(const char *path, const char *arg0, ... /*, (char *)0, char *const envp[]*/);
typedef int (*rtr_execlp_t)(const char *file, const char *arg0, ... /*, (char *)0 */);
typedef int (*rtr_execv_t)(const char *path, char *const argv[]);
#ifndef __APPLE__
typedef int (*rtr_execvpe_t)(const char *file, char *const argv[], char *const envp[]);
typedef int (*rtr_execveat_t)(int dirfd, const char *pathname, char *const argv[], char *const envp[], int flags);
typedef int (*rtr_fexecve_t)(int fd, char *const argv[], char *const envp[]);
#endif
typedef int (*rtr_system_t)(const char *command);

RETRACE_DECL(execl);
RETRACE_DECL(execvp);
RETRACE_DECL(execve);
RETRACE_DECL(execle);
RETRACE_DECL(execlp);
RETRACE_DECL(execv);
#ifndef __APPLE__
RETRACE_DECL(execvpe);
RETRACE_DECL(execveat);
RETRACE_DECL(fexecve);
#endif
RETRACE_DECL(system);

extern char **environ;

#endif /* __RETRACE_EXEC_H__ */
