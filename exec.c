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
#include <unistd.h>

int RETRACE_IMPLEMENTATION(system)(const char *command)
{
	rtr_system_t real_system = RETRACE_GET_REAL(system);
	trace_printf(1, "system(\"%s\");\n", command);
	return real_system(command);
}

RETRACE_REPLACE(system)

int RETRACE_IMPLEMENTATION(execl)(const char *path, const char *arg0, ... /*, (char *)0 */)
{
	rtr_execv_t real_execv = RETRACE_GET_REAL(execv);

	va_list arglist;
	va_start(arglist, arg0);

	char *p = NULL;
	int i = 0;
	int argsize = 1;

	while ((p = va_arg(arglist, char *)) != NULL)
		argsize++;

	const char **argv = malloc(argsize * sizeof(char *));

	va_start(arglist, arg0);

	argv[i] = arg0;
	argv[argsize] = NULL;

	trace_printf(1, "execl(\"%s\"", path);

	while ((p = va_arg(arglist, char *))) {
		argv[++i] = p;
		trace_printf(0, ", \"%s\"", p);
	}

	trace_printf(0, ", 0);\n");

	va_end(arglist);

	set_tracing_enabled(0);

	return real_execv(path, (char *const *) argv);
}

RETRACE_REPLACE(execl)

int RETRACE_IMPLEMENTATION(execv)(const char *path, char *const argv[])
{
	rtr_execv_t real_execv = RETRACE_GET_REAL(execv);

	int i;

	trace_printf(1, "execv(\"%s\"", path);

	for (i = 0;; i++) {
		if (argv[i] == NULL)
			break;

		trace_printf(0, ", \"%s\"", argv[i]);
	}

	trace_printf(0, ", NULL);\n");

	set_tracing_enabled(0);

	return real_execv(path, argv);
}

RETRACE_REPLACE(execv)

int RETRACE_IMPLEMENTATION(execle)(const char *path,
				   const char *arg0,
				   ... /*, (char *)0, char *const envp[]*/)
{
	rtr_execve_t real_execve = RETRACE_GET_REAL(execve);

	va_list arglist;
	va_start(arglist, arg0);

	char *p = NULL;
	int i = 0;
	int argsize = 1;

	while ((p = va_arg(arglist, char *)) != NULL)
		argsize++;

	const char **argv = malloc(argsize * sizeof(char *));

	va_start(arglist, arg0);

	argv[i] = arg0;
	argv[argsize] = NULL;

	trace_printf(1, "execle(\"%s\"", path);

	while ((p = va_arg(arglist, char *)) != NULL) {
		argv[++i] = p;
		trace_printf(0, ", \"%s\"", p);
	}

	trace_printf(0, ", envp);\n");

	char *const *envp = va_arg(arglist, char **);

	trace_printf(1, "char *envp[]=\n");
	trace_printf(1, "{\n");

	for (i = 0;; i++) {
		if (envp[i] == NULL)
			break;

		trace_printf(1, "\t\"%s\",\n", envp[i]);
	}

	trace_printf(1, "\t0\n");
	trace_printf(1, "}\n");

	va_end(arglist);

	set_tracing_enabled(0);

	return real_execve(path, (char *const *) argv, envp);
}

RETRACE_REPLACE(execle)

int RETRACE_IMPLEMENTATION(execve)(const char *path, char *const argv[], char *const envp[])
{
	rtr_execve_t real_execve = RETRACE_GET_REAL(execve);

	int i;

	trace_printf(1, "execve(\"%s\"", path);

	for (i = 0;; i++) {
		if (argv[i] == NULL)
			break;

		trace_printf(0, ", \"%s\"", argv[i]);
	}

	trace_printf(0, ", envp);\n");

	trace_printf(1, "char *envp[]=\n");
	trace_printf(1, "{\n");

	for (i = 0;; i++) {
		if (envp[i] == NULL)
			break;

		trace_printf(1, "\t\"%s\",\n", envp[i]);
	}

	trace_printf(1, "\tNULL\n");
	trace_printf(1, "}\n");

	set_tracing_enabled(0);

	return real_execve(path, argv, envp);
}

RETRACE_REPLACE(execve)

int RETRACE_IMPLEMENTATION(execlp)(const char *file, const char *arg0, ... /*, (char *)0 */)
{
	rtr_execvp_t real_execvp = RETRACE_GET_REAL(execvp);

	va_list arglist;
	va_start(arglist, arg0);

	char *p = NULL;
	int i = 0;
	int argsize = 1;

	while ((p = va_arg(arglist, char *)) != NULL)
		argsize++;

	const char **argv = malloc(argsize * sizeof(char *));

	va_start(arglist, arg0);

	argv[i] = arg0;
	argv[argsize] = NULL;

	trace_printf(1, "execlp(\"%s\"", file);

	while ((p = va_arg(arglist, char *))) {
		argv[++i] = p;
		trace_printf(0, ", \"%s\"", p);
	}

	trace_printf(0, ", 0);\n");

	va_end(arglist);

	set_tracing_enabled(0);

	int ret = real_execvp(file, (char *const *) argv);

	return(ret);
}

RETRACE_REPLACE(execlp)

int RETRACE_IMPLEMENTATION(execvp)(const char *file, char *const argv[])
{
	rtr_execvp_t real_execvp = RETRACE_GET_REAL(execvp);

	int i;

	trace_printf(1, "execvp(\"%s\"", file);

	for (i = 0;; i++) {
		if (argv[i] == NULL)
			break;

		trace_printf(0, ", \"%s\"", argv[i]);
	}

	trace_printf(0, ", NULL);\n");

	set_tracing_enabled(0);

	return real_execvp(file, argv);
}

RETRACE_REPLACE(execvp)

#ifndef __APPLE__
int RETRACE_IMPLEMENTATION(execvpe)(const char *file, char *const argv[], char *const envp[])
{
	rtr_execvpe_t real_execvpe = RETRACE_GET_REAL(execvpe);

	int i;

	trace_printf(1, "execvpe(\"%s\"", file);

	for (i = 0;; i++) {
		if (argv[i] == NULL)
			break;

		trace_printf(0, ", \"%s\"", argv[i]);
	}

	trace_printf(0, ", envp);\n");

	trace_printf(1, "char *envp[]=\n");
	trace_printf(1, "{\n");

	for (i = 0;; i++) {
		if (envp[i] == NULL)
			break;

		trace_printf(1, "\t\"%s\",\n", envp[i]);
	}

	trace_printf(1, "\tNULL\n");
	trace_printf(1, "}\n");

	set_tracing_enabled(0);

	return real_execvpe(file, argv, envp);
}

RETRACE_REPLACE(execvpe)

int RETRACE_IMPLEMENTATION(execveat)(
  int dirfd, const char *pathname, char *const argv[], char *const envp[], int flags)
{
	rtr_execveat_t real_execveat = RETRACE_GET_REAL(execveat);

	int i;

	trace_printf(1, "execveat(%d, \"%s\"", dirfd, pathname);

	for (i = 0;; i++) {
		if (argv[i] == NULL)
			break;

		trace_printf(0, ", \"%s\"", argv[i]);
	}

	trace_printf(0, ", envp, %d);\n", flags);

	trace_printf(1, "char *envp[]=\n");
	trace_printf(1, "{\n");

	for (i = 0;; i++) {
		if (envp[i] == NULL)
			break;

		trace_printf(1, "\t\"%s\",\n", envp[i]);
	}

	trace_printf(1, "\tNULL\n");
	trace_printf(1, "}\n");

	set_tracing_enabled(0);

	return real_execveat(dirfd, pathname, argv, envp, flags);
}

RETRACE_REPLACE(execveat)

int RETRACE_IMPLEMENTATION(fexecve)(int fd, char *const argv[], char *const envp[])
{
	int i;
	rtr_fexecve_t real_fexecve;

	real_fexecve = RETRACE_GET_REAL(fexecve);

	trace_printf(1, "fexecve(%d", fd);

	for (i = 0;; i++) {
		if (argv[i] == NULL)
			break;

		trace_printf(0, ", \"%s\"", argv[i]);
	}

	trace_printf(0, ", envp);\n");

	trace_printf(1, "char *envp[]=\n");
	trace_printf(1, "{\n");

	for (i = 0;; i++) {
		if (envp[i] == NULL)
			break;

		trace_printf(1, "\t\"%s\",\n", envp[i]);
	}

	trace_printf(1, "\tNULL\n");
	trace_printf(1, "}\n");

	set_tracing_enabled(0);

	return real_fexecve(fd, argv, envp);
}

RETRACE_REPLACE(fexecve)

#endif
