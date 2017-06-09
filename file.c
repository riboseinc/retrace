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

        if (fd > 0)
		fd = real_fileno(stream);

	descriptor_info_t *di = file_descriptor_get(fd);
	if (di && di->location)
		trace_printf(1, "fclose(%d); [to: \"%s\"]\n", fd, di->location);
	else
		trace_printf(1, "fclose(%d);\n", fd);

	file_descriptor_remove(fd);

	return real_fclose(stream);
}

RETRACE_REPLACE(fclose)

FILE *RETRACE_IMPLEMENTATION(fopen)(const char *file, const char *mode)
{
	real_fopen = RETRACE_GET_REAL(fopen);
	real_fileno = RETRACE_GET_REAL(fileno);
	int fd = 0;

	FILE *ret = real_fopen(file, mode);

	if (ret)
		fd = real_fileno(ret);

	if (fd > 0) {
		file_descriptor_update(
			fd, FILE_DESCRIPTOR_TYPE_FILE, file, 0);
	}

	trace_printf(1, "fopen(\"%s\", \"%s\"); [%d]\n", file, mode, fd);

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

int RETRACE_IMPLEMENTATION(open)(const char *pathname, int flags, mode_t mode)
{
	real_open = RETRACE_GET_REAL(open);

	int fd = real_open (pathname, flags, mode);

	trace_printf(1, "open(%s, %u, %u) [return: fd]\n", pathname, flags, mode, fd);

	if (fd > 0) {
		file_descriptor_update(
			fd, FILE_DESCRIPTOR_TYPE_FILE, pathname, 0);
	}

	return fd;
}

RETRACE_REPLACE(open)

size_t RETRACE_IMPLEMENTATION(fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	int i;
	int r;
	int fd;
	real_fwrite = RETRACE_GET_REAL(fwrite);
	real_fileno = RETRACE_GET_REAL(fileno);

	r = real_fwrite(ptr, size, nmemb, stream);

	if(get_tracing_enabled()) {
		int old_tracing_enabled = set_tracing_enabled(0);

	        if (stream)
			fd = real_fileno(stream);

		descriptor_info_t *di = file_descriptor_get(fd);
	        if (di && di->location)
			trace_printf(1, "fwrite(%p, %u, %u, %p) [to: \"%s\", return: %u]\n", ptr, size, nmemb, stream, di->location, r);
		else
			trace_printf(1, "fwrite(%p, %u, %u, %p) [return: %u]\n", ptr, size, nmemb, stream, r);

		for (i = 0; i < nmemb; i++)
			trace_dump_data(ptr + i, size);

		set_tracing_enabled(old_tracing_enabled);
	}

	return r;
}

RETRACE_REPLACE(fwrite)

size_t RETRACE_IMPLEMENTATION(fread)(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	int i;
	int r;
	int fd;
	real_fread = RETRACE_GET_REAL(fread);
	real_fileno = RETRACE_GET_REAL(fileno);

	r = real_fread(ptr, size, nmemb, stream);

	if(get_tracing_enabled()) {
		int old_tracing_enabled = set_tracing_enabled(0);

		if (stream)
			fd = real_fileno(stream);

		descriptor_info_t *di = file_descriptor_get(fd);
		if (di && di->location)
		        trace_printf(1, "fread(%p, %u, %u, %p) [to: \"%s\", return: %u]\n", ptr, size, nmemb, stream, di->location, r);
		else
			trace_printf(1, "fread(%p, %u, %u, %p) [return: %u]\n", ptr, size, nmemb, stream, r);

		for (i = 0; i < r; i++) 
			trace_dump_data(ptr + i, size);

		set_tracing_enabled(old_tracing_enabled);
	}

        return r;
}

RETRACE_REPLACE(fread)

int RETRACE_IMPLEMENTATION(fputc)(int c, FILE *stream)
{
	int r;
	int fd;
	real_fputc = RETRACE_GET_REAL(fputc);
	real_fileno = RETRACE_GET_REAL(fileno);

	r = real_fputc(c, stream);

	if(get_tracing_enabled()) {
		int old_tracing_enabled = set_tracing_enabled(0);

		if (stream)
			fd = real_fileno(stream);

		descriptor_info_t *di = file_descriptor_get(fd);
		if (di && di->location)
		        trace_printf(1, "fputc('%c'(%d), %p) [to: \"%s\", return: %d]\n", c, c, stream, di->location, r);
		else
			trace_printf(1, "fputc('%c'(%d), %p) [return: %d]\n", c, c, stream, r);

		set_tracing_enabled(old_tracing_enabled);
	}

        return r;
}

RETRACE_REPLACE(fputc)

int RETRACE_IMPLEMENTATION(fputs)(const char *s, FILE *stream)
{
	int r;
	int fd;
	real_fputs = RETRACE_GET_REAL(fputs);
	real_fileno = RETRACE_GET_REAL(fileno);

	r = real_fputs(s, stream);

	if(get_tracing_enabled()) {
		int old_tracing_enabled = set_tracing_enabled(0);

		if (stream)
			fd = real_fileno(stream);

		descriptor_info_t *di = file_descriptor_get(fd);
		if (di && di->location)
		        trace_printf(1, "fputs(\"%s\", %p) [to: \"%s\", return: %d]\n", s, stream, di->location, r);
		else
			trace_printf(1, "fputs(\"%s\", %p) [return: %d]\n", s, stream, r);

		set_tracing_enabled(old_tracing_enabled);
	}

        return r;
}

RETRACE_REPLACE(fputs)

int RETRACE_IMPLEMENTATION(fgetc)(FILE *stream)
{
	int r;
	int fd;
	real_fgetc = RETRACE_GET_REAL(fgetc);
	real_fileno = RETRACE_GET_REAL(fileno);

	r = real_fgetc(stream);

	if(get_tracing_enabled()) {
		int old_tracing_enabled = set_tracing_enabled(0);

		if (stream)
			fd = real_fileno(stream);

		descriptor_info_t *di = file_descriptor_get(fd);
		if (di && di->location)
		        trace_printf(1, "fgetc(%p) [to: \"%s\", return: '%c'(%d)]\n", stream, di->location, r, r);
		else
			trace_printf(1, "fgetc(%p) [return: '%c'(%d)])\n", stream, r, r);

		set_tracing_enabled(old_tracing_enabled);
	}

        return r;
}

RETRACE_REPLACE(fgetc)

char* RETRACE_IMPLEMENTATION(fgets)(char *s, int size, FILE *stream)
{
	int fd;
	real_fgets = RETRACE_GET_REAL(fgets);
	real_fileno = RETRACE_GET_REAL(fileno);

	real_fgets(s, size, stream);

	if(get_tracing_enabled()) {
		int old_tracing_enabled = set_tracing_enabled(0);

		if (stream)
			fd = real_fileno(stream);

		descriptor_info_t *di = file_descriptor_get(fd);
		if (di && di->location)
		        trace_printf(1, "fgets(\"%s\", %d, %p) [to: \"%s\"]\n", s, size, stream, di->location);
		else
			trace_printf(1, "fgets(\"%s\", %d, %p)\n", s, size, stream);

		set_tracing_enabled(old_tracing_enabled);
	}

        return s;
}

RETRACE_REPLACE(fgets)


