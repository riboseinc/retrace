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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <unistd.h>

#include "common.h"
#include "file.h"
#include "str.h"

int RETRACE_IMPLEMENTATION(stat)(const char *path, struct stat *buf)
{
	real_stat = RETRACE_GET_REAL(stat);

	trace_printf(1, "stat(\"%s\", \"\");\n", path);
	return real_stat(path, buf);
}

RETRACE_REPLACE(stat)

int RETRACE_IMPLEMENTATION(chmod)(const char *path, mode_t mode)
{
	real_chmod = RETRACE_GET_REAL(chmod);

	char perm[10];

	trace_mode(mode, perm);

	trace_printf(1, "chmod(\"%s\", %o); [%s]\n", path, mode, perm);
	return real_chmod(path, mode);
}

RETRACE_REPLACE(chmod)

int RETRACE_IMPLEMENTATION(fchmod)(int fd, mode_t mode)
{
	real_fchmod = RETRACE_GET_REAL(fchmod);

	char perm[10];

	trace_mode(mode, perm);

	trace_printf(1, "fchmod(%d, %o); [%s]\n", fd, mode, perm);
	return real_fchmod(fd, mode);
}

RETRACE_REPLACE(fchmod)

int RETRACE_IMPLEMENTATION(fileno)(FILE *stream)
{
	real_fileno = RETRACE_GET_REAL(fileno);
	int fd = real_fileno(stream);

	trace_printf(1, "fileno(%d);\n", fd);
	return real_fileno(stream);
}

RETRACE_REPLACE(fileno)

int RETRACE_IMPLEMENTATION(fseek)(FILE *stream, long offset, int whence)
{
	real_fseek = RETRACE_GET_REAL(fseek);
	real_fileno = RETRACE_GET_REAL(fileno);
	int fd = real_fileno(stream);

	trace_printf(1, "fseek(%d, %lx, ", fd, offset);

	if (whence == 0)
		trace_printf(0, "SEEK_SET");
	else if (whence == 1)
		trace_printf(0, "SEEK_CUR");
	else if (whence == 2)
		trace_printf(0, "SEEK_END");
	else
		trace_printf(0, "UNDEFINED");

	trace_printf(0, ");\n");

	return real_fseek(stream, offset, whence);
}

RETRACE_REPLACE(fseek)

int RETRACE_IMPLEMENTATION(fclose)(FILE *stream)
{
	real_fclose = RETRACE_GET_REAL(fclose);
	real_fileno = RETRACE_GET_REAL(fileno);
	int fd = real_fileno(stream);

	trace_printf(1, "fclose(%d);\n", fd);
	return real_fclose(stream);
}

RETRACE_REPLACE(fclose)

FILE *RETRACE_IMPLEMENTATION(fopen)(const char *file, const char *mode)
{
	real_fopen = RETRACE_GET_REAL(fopen);
	real_fileno = RETRACE_GET_REAL(fileno);
	real_strcmp = RETRACE_GET_REAL(strcmp);
	int fd = 0;
	int did_redirect = 0;
	FILE *ret;
        char *match_file = NULL;
        char *redirect_file = NULL;

	if (get_tracing_enabled() && file) {
		rtr_config config = NULL;

		while (rtr_get_config_multiple (&config, "fopen",
				ARGUMENT_TYPE_STRING,
				ARGUMENT_TYPE_STRING,
				ARGUMENT_TYPE_END,
				&match_file,
				&redirect_file)) {

			if (real_strcmp (match_file, file) == 0) {
				did_redirect = 1;

				ret = real_fopen(redirect_file, mode);

				if (config)
					rtr_confing_close (config);

				break;
			}

			free (match_file);
			free (redirect_file);
		}
	}

	if (!did_redirect)
		ret = real_fopen(file, mode);

	if (ret)
		fd = real_fileno(ret);

	trace_printf(1, "fopen(\"%s\", \"%s\"); [%d]\n", did_redirect ? redirect_file : file , mode, fd);

	if (did_redirect) {
		free (match_file);
	        free (redirect_file);
	}


	return (ret);
}

RETRACE_REPLACE(fopen)

DIR *RETRACE_IMPLEMENTATION(opendir)(const char *dirname)
{
	real_opendir = RETRACE_GET_REAL(opendir);
	trace_printf(1, "opendir(\"%s\");\n", dirname);
	return real_opendir(dirname);
}

RETRACE_REPLACE(opendir)

int RETRACE_IMPLEMENTATION(closedir)(DIR *dirp)
{
	real_closedir = RETRACE_GET_REAL(closedir);
	trace_printf(1, "closedir();\n");
	return real_closedir(dirp);
}

RETRACE_REPLACE(closedir)

int RETRACE_IMPLEMENTATION(close)(int fd)
{
	real_close = dlsym(RTLD_NEXT, "close");

	descriptor_info_t *di = file_descriptor_get(fd);

	if (di && di->location) {
		trace_printf(1, "close(%d) [was pointing to %s];\n", fd, di->location);
	} else {
		trace_printf(1, "close(%d);\n", fd);
	}

	file_descriptor_remove(fd);

	return real_close(fd);
}

RETRACE_REPLACE(close)

int RETRACE_IMPLEMENTATION(dup)(int oldfd)
{
	real_dup = RETRACE_GET_REAL(dup);
	trace_printf(1, "dup(%d)\n", oldfd);
	return real_dup(oldfd);
}

RETRACE_REPLACE(dup)

int RETRACE_IMPLEMENTATION(dup2)(int oldfd, int newfd)
{
	real_dup2 = RETRACE_GET_REAL(dup2);
	trace_printf(1, "dup2(%d, %d)\n", oldfd, newfd);
	return real_dup2(oldfd, newfd);
}

RETRACE_REPLACE(dup2)
