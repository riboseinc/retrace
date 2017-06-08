#ifndef __RETRACE_EXEC_H__
#define __RETRACE_EXEC_H__

typedef int (*rtr_execl_t)(const char *path, const char *arg0, ... /*, (char *)0 */);
typedef int (*rtr_execvp_t)(const char *path, char *const argv[]);
typedef int (*rtr_execve_t)(const char *path, char *const argv[], char *const envp[]);
typedef int (*rtr_execle_t)(const char *path, const char *arg0, ... /*, (char *)0, char *const envp[]*/);
typedef int (*rtr_execlp_t)(const char *file, const char *arg0, ... /*, (char *)0 */);
typedef int (*rtr_execv_t)(const char *path, char *const argv[]);
typedef int (*rtr_execvpe_t)(const char *file, char *const argv[], char *const envp[]);
typedef int (*rtr_execveat_t)(int dirfd, const char *pathname, char *const argv[], char *const envp[], int flags);
typedef int (*rtr_fexecve_t)(int fd, char *const argv[], char *const envp[]);
typedef int (*rtr_system_t)(const char *command);

rtr_execvp_t real_execvp;
rtr_execve_t real_execve;
rtr_execv_t real_execv;
rtr_execvpe_t real_execvpe;
rtr_execveat_t real_execveat;
rtr_fexecve_t real_fexecve;
rtr_system_t real_system;

#endif /* __RETRACE_EXEC_H__ */
