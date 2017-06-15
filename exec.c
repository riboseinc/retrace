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
	rtr_system_t real_system;

	real_system = RETRACE_GET_REAL(system);

	trace_printf(1, "system(\"%s\");\n", command);

	return real_system(command);
}

RETRACE_REPLACE(system)

int RETRACE_IMPLEMENTATION(execl)(const char *path, const char *arg0, ... /*, (char *)0 */)
{
	int i = 0;
	int argsize = 1;
	const char **argv;
	char *p = NULL;
	rtr_execv_t real_execv;
	va_list arglist;
	int r;
	int old_trace_state;

	real_execv = RETRACE_GET_REAL(execv);

	va_start(arglist, arg0);

	while ((p = va_arg(arglist, char *)) != NULL)
		argsize++;

	argv = malloc(argsize * sizeof(char *));

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

	old_trace_state = trace_disable();

	r = real_execv(path, (char *const *)argv);

	trace_restore(old_trace_state);

	return (r);
}

RETRACE_REPLACE(execl)

int RETRACE_IMPLEMENTATION(execv)(const char *path, char *const argv[])
{
	int i;
	rtr_execv_t real_execv;
	int r;
	int old_trace_state;

	real_execv = RETRACE_GET_REAL(execv);

	trace_printf(1, "execv(\"%s\"", path);

	for (i = 0;; i++) {
		if (argv[i] == NULL)
			break;

		trace_printf(0, ", \"%s\"", argv[i]);
	}

	trace_printf(0, ", NULL);\n");

	old_trace_state = trace_disable();

	r = real_execv(path, argv);

	trace_restore(old_trace_state);

	return (r);
}

RETRACE_REPLACE(execv)

int RETRACE_IMPLEMENTATION(execle)(const char *path,
		const char *arg0,
		... /*, (char *)0, char *const envp[]*/)
{
	int argsize = 1;
	int i = 0;
	const char **argv;
	char *const *envp;
	char *p = NULL;
	rtr_execve_t real_execve;
	va_list arglist;
	int r;
	int old_trace_state;

	real_execve = RETRACE_GET_REAL(execve);

	va_start(arglist, arg0);

	while ((p = va_arg(arglist, char *)) != NULL)
		argsize++;

	argv = malloc(argsize * sizeof(char *));

	va_start(arglist, arg0);

	argv[i] = arg0;
	argv[argsize] = NULL;

	trace_printf(1, "execle(\"%s\"", path);

	while ((p = va_arg(arglist, char *)) != NULL) {
		argv[++i] = p;
		trace_printf(0, ", \"%s\"", p);
	}

	trace_printf(0, ", envp);\n");

	envp = va_arg(arglist, char **);

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

	old_trace_state = trace_disable();

	r = real_execve(path, (char *const *)argv, envp);

	trace_restore(old_trace_state);

	return (r);
}

RETRACE_REPLACE(execle)

int RETRACE_IMPLEMENTATION(execve)(const char *path, char *const argv[], char *const envp[])
{
	int i;
	rtr_execve_t real_execve;
	int r;
	int old_trace_state;


	real_execve = RETRACE_GET_REAL(execve);

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

	old_trace_state = trace_disable();

	r = real_execve(path, argv, envp);

	trace_restore(old_trace_state);

	return (r);
}

RETRACE_REPLACE(execve)

int RETRACE_IMPLEMENTATION(execlp)(const char *file, const char *arg0, ... /*, (char *)0 */)
{
	int argsize = 1;
	int i = 0;
	int r;
	const char **argv;
	char *p = NULL;
	rtr_execvp_t real_execvp;
	va_list arglist;
	int old_trace_state;


	real_execvp = RETRACE_GET_REAL(execvp);

	va_start(arglist, arg0);

	while ((p = va_arg(arglist, char *)) != NULL)
		argsize++;

	argv = malloc(argsize * sizeof(char *));

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

	old_trace_state = trace_disable();

	r = real_execvp(file, (char *const *)argv);

	trace_restore(old_trace_state);

	return (r);
}

RETRACE_REPLACE(execlp)

int RETRACE_IMPLEMENTATION(execvp)(const char *file, char *const argv[])
{
	int i;
	rtr_execvp_t real_execvp;
	int r;
	int old_trace_state;


	real_execvp = RETRACE_GET_REAL(execvp);

	trace_printf(1, "execvp(\"%s\"", file);

	for (i = 0;; i++) {
		if (argv[i] == NULL)
			break;

		trace_printf(0, ", \"%s\"", argv[i]);
	}

	trace_printf(0, ", NULL);\n");

	old_trace_state = trace_disable();

	r = real_execvp(file, argv);

	trace_restore(old_trace_state);

	return (r);
}

RETRACE_REPLACE(execvp)

#ifndef __APPLE__
int RETRACE_IMPLEMENTATION(execvpe)(const char *file, char *const argv[], char *const envp[])
{
	int i;
	rtr_execvpe_t real_execvpe;
	int r;
	int old_trace_state;


	real_execvpe = RETRACE_GET_REAL(execvpe);

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

	old_trace_state = trace_disable();

	r = real_execvpe(file, argv, envp);

	trace_restore(old_trace_state);

	return (r);
}

RETRACE_REPLACE(execvpe)

int RETRACE_IMPLEMENTATION(execveat)(int dirfd, const char *pathname,
		char *const argv[], char *const envp[], int flags)
{
	int i;
	rtr_execveat_t real_execveat;
	int r;
	int old_trace_state;

	real_execveat = RETRACE_GET_REAL(execveat);

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

	old_trace_state = trace_disable();

	r = real_execveat(dirfd, pathname, argv, envp, flags);

	trace_restore(old_trace_state);

	return (r);
}

RETRACE_REPLACE(execveat)

int RETRACE_IMPLEMENTATION(fexecve)(int fd, char *const argv[], char *const envp[])
{
	int i;
	int r;
	int old_trace_state;

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


	old_trace_state = trace_disable();

	r = fexecve(fd, argv, envp);

	trace_restore(old_trace_state);

	return r;
}

RETRACE_REPLACE(fexecve)

#endif /* !__APPLE__ */
