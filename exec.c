/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"
#include "exec.h"
#include "malloc.h"
#include "str.h"
#include <unistd.h>


/* Duplicates a string array adding extra spaces,
 * returns the new array that must be free'd
 */
static char **
duplicate_string_array(const char **array, int extra_space, int *count)
{
	const char **s;
	char **ret;

	*count = extra_space + 1;

	if (array)
		for (s = array; *s; s++)
			(*count)++;


	/* relying in the fact that calloc initializes memory to zero */
	ret = (char **) real_calloc(*count, sizeof(char *));

	if (ret) {
		int i;

		for (i = 0; i < (*count) - extra_space; i++)
			ret[i] = (char *) array[i];
	}

	return ret;
}

static char*
find_environment_var(const char *var)
{
	char **s;
	int len;

	len = real_strlen(var);

	for (s = environ; *s; s++)
		if (real_strncmp(*s, var, len) == 0)
			return *s;

	return NULL;
}

static char**
inject_retrace_env_vars(const char **env)
{
	static int follow_children = -1;
	char **new_env = NULL;

	if (follow_children == -1) {
		follow_children = 0;
		if (rtr_get_config_single("forcefollowexec", ARGUMENT_TYPE_END))
			follow_children = 1;
	}

	if (follow_children) {
		int size;
#ifdef __APPLE__
		char *retrace_env_var = "DYLD_INSERT_LIBRARIES";
#else
		char *retrace_env_var = "LD_PRELOAD";
#endif
		char *retrace_config_var = "RETRACE_CONFIG";

		char *var_to_copy;

		new_env = duplicate_string_array((const char **) env, 2, &size);

		var_to_copy = find_environment_var(retrace_env_var);

		if (new_env && var_to_copy)
			new_env[size - 3] = var_to_copy;

		var_to_copy = find_environment_var(retrace_config_var);
		if (new_env && var_to_copy)
			new_env[size - 2] = var_to_copy;
	}

	return new_env;
}


int RETRACE_IMPLEMENTATION(system)(const char *command)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&command};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "system";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.event_flags = EVENT_FLAGS_PRINT_BEFORE;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_system(command);
	if (r != 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(system, int, (const char *command), (command))

int
execl_v(const char *path, const char *arg0, va_list ap)
{
	va_list ap_copy;
	int nargs, i, r, old_trace_state;
	char **args;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&path, &args};

	old_trace_state = trace_disable();

	va_copy(ap_copy, ap);
	for (nargs = 2; va_arg(ap_copy, char *) != NULL; ++nargs)
		;
	va_end(ap_copy);

	args = alloca(nargs * sizeof(void *));
	args[0] = (char *)arg0;
	for (i = 1; i < nargs; i++)
		args[i] = va_arg(ap, char *);

	trace_restore(old_trace_state);


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "execl";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.event_flags = EVENT_FLAGS_PRINT_BEFORE;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_execv(path, args);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

int RETRACE_IMPLEMENTATION(execl)(const char *path, const char *arg0, ... /*, (char *)0 */)
{
	int r;
	char *parg;
	va_list ap;

	va_start(ap, arg0);
	r = execl_v(path, arg0, ap);
	va_end(ap);

	return (r);
}

RETRACE_REPLACE_V(execl, int, (const char *path, const char *arg0, ...), arg0, execl_v, (path, arg0, ap))

int RETRACE_IMPLEMENTATION(execv)(const char *path, char *const argv[])
{
	int r;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&path, &argv};


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "execv";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.event_flags = EVENT_FLAGS_PRINT_BEFORE;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_execv(path, argv);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(execv, int, (const char *path, char *const argv[]), (path, argv))


int
execle_v(const char *path, const char *arg0, va_list ap)
{
	va_list ap_copy;
	int nargs, i, r, old_trace_state;
	char **args;
	char **envp;
	char **new_envp;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&path, &args, &new_envp};
	static int follow_children = -1;

	old_trace_state = trace_disable();

	va_copy(ap_copy, ap);
	for (nargs = 2; va_arg(ap_copy, char *) != NULL; ++nargs)
		;

	envp = va_arg(ap_copy, char **);

	va_end(ap_copy);

	trace_restore(old_trace_state);
	new_envp = inject_retrace_env_vars((const char **) envp);
	if (!new_envp)
		new_envp = (char **) envp;
	old_trace_state = trace_disable();

	va_copy(ap_copy, ap);
	args = alloca(nargs * sizeof(void *));
	args[0] = (char *)arg0;
	for (i = 1; i < nargs; i++)
		args[i] = va_arg(ap, char *);

	trace_restore(old_trace_state);

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "execle";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.event_flags = EVENT_FLAGS_PRINT_BEFORE;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_execve(path, args, envp);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	if (new_envp && new_envp != envp)
		real_free(new_envp);

	return (r);
}

int RETRACE_IMPLEMENTATION(execle)(const char *path,
		const char *arg0,
		... /*, (char *)0, char *const envp[]*/)
{
	int r;
	const char *parg, *penv;
	va_list ap;

	va_start(ap, arg0);
	r = execle_v(path, arg0, ap);
	va_end(ap);

	return (r);
}

RETRACE_REPLACE_V(execle, int, (const char *path, const char *arg0, ...), arg0, execle_v, (path, arg0, ap))

int RETRACE_IMPLEMENTATION(execve)(const char *path, char *const argv[], char *const envp[])
{
	int r;
	char **new_envp;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&path, &argv, &new_envp};


	new_envp = inject_retrace_env_vars((const char **) envp);
	if (!new_envp)
		new_envp = (char **) envp;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "execve";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.event_flags = EVENT_FLAGS_PRINT_BEFORE;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_execve(path, argv, new_envp);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	if (new_envp && new_envp != envp)
		real_free(new_envp);

	return (r);
}

RETRACE_REPLACE(execve, int, (const char *path, char *const argv[], char *const envp[]), (path, argv, envp))


int
execlp_v(const char *file, const char *arg0, va_list ap)
{
	va_list ap_copy;
	int nargs, i, r, old_trace_state;
	char **args;

	old_trace_state = trace_disable();

	va_copy(ap_copy, ap);
	for (nargs = 2; va_arg(ap_copy, char *) != NULL; ++nargs)
		;
	va_end(ap_copy);

	args = alloca(nargs * sizeof(void *));
	args[0] = (char *)arg0;
	for (i = 1; i < nargs; i++)
		args[i] = va_arg(ap, char *);
	r = real_execvp(file, args);

	trace_restore(old_trace_state);
	return (r);
}

int RETRACE_IMPLEMENTATION(execlp)(const char *file, const char *arg0, ... /*, (char *)0 */)
{
	int r;
	char *parg;
	va_list ap;

	va_start(ap, arg0);
	r = execlp_v(file, arg0, ap);
	va_end(ap);

	return (r);
}

RETRACE_REPLACE_V(execlp, int, (const char *file, const char *arg0, ...), arg0, execlp_v, (file, arg0, ap))


int RETRACE_IMPLEMENTATION(execvp)(const char *file, char *const argv[])
{
	int r;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&file, &argv};


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "execvp";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.event_flags = EVENT_FLAGS_PRINT_BEFORE;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_execvp(file, argv);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(execvp, int, (const char *file, char *const argv[]), (file, argv))


#ifndef __APPLE__
int RETRACE_IMPLEMENTATION(execvpe)(const char *file, char *const argv[], char *const envp[])
{
	int r;
	char **new_envp;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&file, &argv, &new_envp};

	new_envp = inject_retrace_env_vars((const char **) envp);
	if (!new_envp)
		new_envp = (char **) envp;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "execvpe";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.event_flags = EVENT_FLAGS_PRINT_BEFORE;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_execvpe(file, argv, envp);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	if (new_envp && new_envp != envp)
		real_free(new_envp);

	return (r);
}

RETRACE_REPLACE(execvpe, int, (const char *file, char *const argv[], char *const envp[]), (file, argv, envp))


int RETRACE_IMPLEMENTATION(execveat)(int dirfd, const char *pathname,
		char *const argv[], char *const envp[], int flags)
{
	int r;
	char **new_envp;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR,
					  PARAMETER_TYPE_STRING,
					  PARAMETER_TYPE_STRING_ARRAY,
					  PARAMETER_TYPE_STRING_ARRAY,
					  PARAMETER_TYPE_INT,
					  PARAMETER_TYPE_END};
	void const *parameter_values[] = {&dirfd, &pathname, &argv, &new_envp, &flags};


	new_envp = inject_retrace_env_vars((const char **) envp);
	if (!new_envp)
		new_envp = (char **) envp;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "execveat";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.event_flags = EVENT_FLAGS_PRINT_BEFORE;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_execveat(dirfd, pathname, argv, envp, flags);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	if (new_envp && new_envp != envp)
		real_free(new_envp);

	return (r);
}

RETRACE_REPLACE(execveat, int,
	(int dirfd, const char *pathname, char *const argv[],
	    char *const envp[], int flags),
	(dirfd, pathname, argv, envp, flags))


int RETRACE_IMPLEMENTATION(fexecve)(int fd, char *const argv[], char *const envp[])
{
	int r;
	char **new_envp;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fd, &argv, &new_envp};

	new_envp = inject_retrace_env_vars((const char **) envp);
	if (!new_envp)
		new_envp = (char **) envp;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fexecve";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.event_flags = EVENT_FLAGS_PRINT_BEFORE;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_fexecve(fd, argv, envp);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	if (new_envp && new_envp != envp)
		real_free(new_envp);

	return r;
}

RETRACE_REPLACE(fexecve, int,
	(int fd, char *const argv[], char *const envp[]), (fd, argv, envp))

#endif /* !__APPLE__ */
