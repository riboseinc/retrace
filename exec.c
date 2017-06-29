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
#include <unistd.h>

int RETRACE_IMPLEMENTATION(system)(const char *command)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&command};
	int r;


	event_info.function_name = "system";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_system(command);

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


	event_info.function_name = "execl";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_execv(path, args);

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


	event_info.function_name = "execv";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_execv(path, argv);

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
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&path, &args, &envp};

	old_trace_state = trace_disable();

	va_copy(ap_copy, ap);
	for (nargs = 2; va_arg(ap_copy, char *) != NULL; ++nargs)
		;

	envp = va_arg(ap_copy, char **);

	va_end(ap_copy);

	va_copy(ap_copy, ap);
	args = alloca(nargs * sizeof(void *));
	args[0] = (char *)arg0;
	for (i = 1; i < nargs; i++)
		args[i] = va_arg(ap, char *);

	trace_restore(old_trace_state);


	event_info.function_name = "execle";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_execve(path, args, envp);

	retrace_log_and_redirect_after(&event_info);

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
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&path, &argv, &envp};


	event_info.function_name = "execve";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_execve(path, argv, envp);

	retrace_log_and_redirect_after(&event_info);

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


	event_info.function_name = "execvp";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_execvp(file, argv);

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(execvp, int, (const char *file, char *const argv[]), (file, argv))


#ifndef __APPLE__
int RETRACE_IMPLEMENTATION(execvpe)(const char *file, char *const argv[], char *const envp[])
{
	int r;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&file, &argv, &envp};


	event_info.function_name = "execvpe";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_execvpe(file, argv, envp);

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(execvpe, int, (const char *file, char *const argv[], char *const envp[]), (file, argv, envp))


int RETRACE_IMPLEMENTATION(execveat)(int dirfd, const char *pathname,
		char *const argv[], char *const envp[], int flags)
{
	int r;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR,
					  PARAMETER_TYPE_STRING,
					  PARAMETER_TYPE_STRING_ARRAY,
					  PARAMETER_TYPE_STRING_ARRAY,
					  PARAMETER_TYPE_INT,
					  PARAMETER_TYPE_END};
	void const *parameter_values[] = {&dirfd, &pathname, &argv, &envp, &flags};


	event_info.function_name = "execveat";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_execveat(dirfd, pathname, argv, envp, flags);

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(execveat, int,
	(int dirfd, const char *pathname, char *const argv[],
	    char *const envp[], int flags),
	(dirfd, pathname, argv, envp, flags))


int RETRACE_IMPLEMENTATION(fexecve)(int fd, char *const argv[], char *const envp[])
{
	int r;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_STRING_ARRAY, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fd, &argv, &envp};


	event_info.function_name = "fexecve";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_fexecve(fd, argv, envp);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(fexecve, int,
	(int fd, char *const argv[], char *const envp[]), (fd, argv, envp))

#endif /* !__APPLE__ */
