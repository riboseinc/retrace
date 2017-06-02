#ifndef __RETRACE_EXEC_H__
#define __RETRACE_EXEC_H__

typedef int (*rtr_execve_t)(const char *path, char *const argv[], char *const envp[]);
typedef int (*rtr_system_t)(const char *command);

rtr_execve_t real_execve;
rtr_system_t real_system;

#endif /* __RETRACE_EXEC_H__ */
