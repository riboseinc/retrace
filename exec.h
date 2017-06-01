#ifndef __RETRACE_EXEC_H__
#define __RETRACE_EXEC_H__

static int (*real_execve)(const char *path, char *const argv[],
	char *const envp[]);
static int (*real_system)(const char *command);

#endif /* __RETRACE_EXEC_H__ */
