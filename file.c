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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>

#include "file.h"
#include "str.h"

#ifdef __FreeBSD__
#undef fileno
#endif

int RETRACE_IMPLEMENTATION(stat)(const char *path, struct stat *buf)
{
	rtr_stat_t real_stat;
	char perm[10];
	int r;

	real_stat = RETRACE_GET_REAL(stat);

	trace_printf(1, "stat(\"%s\", buf);\n", path);

	r = real_stat(path, buf);

	if (r == 0) {
		trace_printf(1, "struct stat {\n");
		trace_printf(1, "\tst_dev = %lu\n", buf->st_dev);
		trace_printf(1, "\tst_ino = %i\n", buf->st_ino);
		trace_mode(buf->st_mode, perm);
		trace_printf(1, "\tst_mode = %d [%s]\n", buf->st_mode, perm);
		trace_printf(1, "\tst_nlink = %lu\n", buf->st_nlink);
		trace_printf(1, "\tst_uid = %d\n", buf->st_uid);
		trace_printf(1, "\tst_gid = %d\n", buf->st_gid);
		trace_printf(1, "\tst_rdev = %r\n", buf->st_rdev);
		trace_printf(1, "\tst_atime = %lu\n", buf->st_atime);
		trace_printf(1, "\tst_mtime = %lu\n", buf->st_mtime);
		trace_printf(1, "\tst_ctime = %lu\n", buf->st_ctime);
		trace_printf(1, "\tst_size = %zu\n", buf->st_size);
		trace_printf(1, "\tst_blocks = %lu\n", buf->st_blocks);
		trace_printf(1, "\tst_blksize = %lu\n", buf->st_blksize);
#if __APPLE__
		trace_printf(1, "\tst_flags = %d\n", buf->st_flags);
		trace_printf(1, "\tst_gen = %d\n", buf->st_gen);
#endif
		trace_printf(1, "}\n");
	}

	return r;
}

RETRACE_REPLACE(stat)

int RETRACE_IMPLEMENTATION(chmod)(const char *path, mode_t mode)
{
	rtr_chmod_t real_chmod;
	char perm[10];

	real_chmod = RETRACE_GET_REAL(chmod);

	trace_mode(mode, perm);

	trace_printf(1, "chmod(\"%s\", %o); [%s]\n", path, mode, perm);

	return real_chmod(path, mode);
}

RETRACE_REPLACE(chmod)

int RETRACE_IMPLEMENTATION(fchmod)(int fd, mode_t mode)
{
	rtr_fchmod_t real_fchmod;
	char perm[10];

	real_fchmod = RETRACE_GET_REAL(fchmod);

	trace_mode(mode, perm);

	trace_printf(1, "fchmod(%d, %o); [%s]\n", fd, mode, perm);

	return real_fchmod(fd, mode);
}

RETRACE_REPLACE(fchmod)

int RETRACE_IMPLEMENTATION(fileno)(FILE *stream)
{
	int fd;
	rtr_fileno_t real_fileno;

	real_fileno = RETRACE_GET_REAL(fileno);

	fd = real_fileno(stream);

	trace_printf(1, "fileno(%d);\n", fd);

	return real_fileno(stream);
}

RETRACE_REPLACE(fileno)

int RETRACE_IMPLEMENTATION(fseek)(FILE *stream, long offset, int whence)
{
	int fd;
	rtr_fseek_t real_fseek;
	rtr_fileno_t real_fileno;

	real_fseek	= RETRACE_GET_REAL(fseek);
	real_fileno	= RETRACE_GET_REAL(fileno);

	fd = real_fileno(stream);

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
	int fd;
	struct descriptor_info *di;
	rtr_fclose_t real_fclose;
	rtr_fileno_t real_fileno;

	real_fclose = RETRACE_GET_REAL(fclose);
	real_fileno = RETRACE_GET_REAL(fileno);

	fd = real_fileno(stream);

	if (fd > 0)
		fd = real_fileno(stream);

	di = file_descriptor_get(fd);
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
	int did_redirect = 0;
	int fd = 0;
	char *match_file = NULL;
	FILE *ret;
	char *redirect_file = NULL;
	rtr_fopen_t real_fopen;
	rtr_strcmp_t real_strcmp;

	real_fopen	= RETRACE_GET_REAL(fopen);
	real_strcmp	= RETRACE_GET_REAL(strcmp);

	if (get_tracing_enabled() && file) {
		FILE *config = NULL;

		while (1) {
			int r;

			r = rtr_get_config_multiple(&config,
					"fopen",
					ARGUMENT_TYPE_STRING,
					ARGUMENT_TYPE_STRING,
					ARGUMENT_TYPE_END,
					&match_file,
					&redirect_file);
			if (r == 0)
				break;

			if (real_strcmp(match_file, file) == 0) {
				did_redirect = 1;

				ret = real_fopen(redirect_file, mode);

				if (config)
					rtr_config_close(config);

				break;
			}

			free(match_file);
			free(redirect_file);
		}
	}

	if (!did_redirect)
		ret = real_fopen(file, mode);

	trace_printf(1, "fopen(\"%s\", \"%s\"); [%d]\n", did_redirect ? redirect_file : file, mode, fd);

	if (did_redirect) {
		free(match_file);
		free(redirect_file);
	}

	return ret;
}

RETRACE_REPLACE(fopen)

int RETRACE_IMPLEMENTATION(close)(int fd)
{
	struct descriptor_info *di;
	rtr_close_t real_close;

	real_close = RETRACE_GET_REAL(close);

	di = file_descriptor_get(fd);
	if (di && di->location)
		trace_printf(1, "close(%d) [was pointing to %s];\n", fd, di->location);
	else
		trace_printf(1, "close(%d);\n", fd);

	file_descriptor_remove(fd);

	return real_close(fd);
}

RETRACE_REPLACE(close)

int RETRACE_IMPLEMENTATION(dup)(int oldfd)
{
	rtr_dup_t real_dup;

	real_dup = RETRACE_GET_REAL(dup);

	trace_printf(1, "dup(%d)\n", oldfd);

	return real_dup(oldfd);
}

RETRACE_REPLACE(dup)

int RETRACE_IMPLEMENTATION(dup2)(int oldfd, int newfd)
{
	rtr_dup2_t real_dup2;

	real_dup2 = RETRACE_GET_REAL(dup2);

	trace_printf(1, "dup2(%d, %d)\n", oldfd, newfd);

	return real_dup2(oldfd, newfd);
}

RETRACE_REPLACE(dup2)

mode_t RETRACE_IMPLEMENTATION(umask)(mode_t mask)
{
	mode_t old_mask;
	rtr_umask_t real_umask;

	real_umask = RETRACE_GET_REAL(umask);

	old_mask = real_umask(mask);

	trace_printf(1, "umask(%d); [%d]\n", mask, old_mask);

	return old_mask;
}

RETRACE_REPLACE(umask)

int RETRACE_IMPLEMENTATION(mkfifo)(const char *pathname, mode_t mode)
{
	int ret;
	rtr_mkfifo_t real_mkfifo;

	real_mkfifo = RETRACE_GET_REAL(mkfifo);

	ret = real_mkfifo(pathname, mode);

	trace_printf(1, "mkfifo(%s, %d); [%d]\n", pathname, mode, ret);

	return ret;
}

RETRACE_REPLACE(mkfifo)

int RETRACE_IMPLEMENTATION(open)(const char *pathname, int flags, ...)
{
	int fd;
	mode_t mode;
	rtr_open_t real_open;
	va_list arglist;

	real_open = RETRACE_GET_REAL(open);

	va_start(arglist, flags);
	mode = va_arg(arglist, int);

	fd = real_open(pathname, flags, mode);

	va_end(arglist);

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
	size_t i;
	int r, fd;
	struct descriptor_info *di = NULL;
	rtr_fwrite_t real_fwrite;
	rtr_fileno_t real_fileno;

	real_fwrite = RETRACE_GET_REAL(fwrite);
	real_fileno = RETRACE_GET_REAL(fileno);

	r = real_fwrite(ptr, size, nmemb, stream);

	if (get_tracing_enabled()) {
		int old_trace_state;

		old_trace_state = trace_disable();

		fd = real_fileno(stream);
		di = file_descriptor_get(fd);

		if (di && di->location)
			trace_printf(1, "fwrite(%p, %u, %u, %p) [to: \"%s\", return: %u]\n", ptr, size, nmemb, stream, di->location, r);
		else
			trace_printf(1, "fwrite(%p, %u, %u, %p) [return: %u]\n", ptr, size, nmemb, stream, r);

		for (i = 0; i < nmemb; i++)
			trace_dump_data((unsigned char *)ptr + i, size);

		trace_restore(old_trace_state);
	}

	return r;
}

RETRACE_REPLACE(fwrite)

size_t RETRACE_IMPLEMENTATION(fread)(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	int i, r, fd;
	struct descriptor_info *di = NULL;
	rtr_fread_t real_fread;
	rtr_fileno_t real_fileno;

	real_fread	= RETRACE_GET_REAL(fread);
	real_fileno	= RETRACE_GET_REAL(fileno);

	r = real_fread(ptr, size, nmemb, stream);

	if (get_tracing_enabled()) {
		int old_trace_state;

		old_trace_state = trace_disable();

		if (stream) {
			fd = real_fileno(stream);
			di = file_descriptor_get(fd);
		}

		if (di && di->location)
			trace_printf(1, "fread(%p, %u, %u, %p) [to: \"%s\", return: %u]\n", ptr, size, nmemb, stream, di->location, r);
		else
			trace_printf(1, "fread(%p, %u, %u, %p) [return: %u]\n", ptr, size, nmemb, stream, r);

		for (i = 0; i < r; i++)
			trace_dump_data((unsigned char *)ptr + i, size);

		trace_restore(old_trace_state);
	}

	return r;
}

RETRACE_REPLACE(fread)

int RETRACE_IMPLEMENTATION(fputc)(int c, FILE *stream)
{
	int fd;
	int r;
	struct descriptor_info *di = NULL;
	rtr_fputc_t real_fputc;
	rtr_fileno_t real_fileno;

	real_fputc	= RETRACE_GET_REAL(fputc);
	real_fileno	= RETRACE_GET_REAL(fileno);

	r = real_fputc(c, stream);

	if (get_tracing_enabled()) {
		int old_trace_state;

		old_trace_state = trace_disable();

		fd = real_fileno(stream);
		di = file_descriptor_get(fd);

		if (di && di->location)
			trace_printf(1, "fputc('%c'(%d), %p) [to: \"%s\", return: %d]\n", c, c, stream, di->location, r);
		else
			trace_printf(1, "fputc('%c'(%d), %p) [return: %d]\n", c, c, stream, r);

		trace_restore(old_trace_state);
	}

	return r;
}

RETRACE_REPLACE(fputc)

int RETRACE_IMPLEMENTATION(fputs)(const char *s, FILE *stream)
{
	int r, fd;
	struct descriptor_info *di = NULL;
	rtr_fputs_t real_fputs;
	rtr_fileno_t real_fileno;

	real_fputs	= RETRACE_GET_REAL(fputs);
	real_fileno	= RETRACE_GET_REAL(fileno);

	r = real_fputs(s, stream);

	if (get_tracing_enabled()) {
		int old_trace_state;

		old_trace_state = trace_disable();

		fd = real_fileno(stream);
		di = file_descriptor_get(fd);

		if (di && di->location)
			trace_printf(1, "fputs(\"%s\", %p) [to: \"%s\", return: %d]\n", s, stream, di->location, r);
		else
			trace_printf(1, "fputs(\"%s\", %p) [return: %d]\n", s, stream, r);

		trace_restore(old_trace_state);
	}

	return r;
}

RETRACE_REPLACE(fputs)

int RETRACE_IMPLEMENTATION(fgetc)(FILE *stream)
{
	int r;
	int fd;
	struct descriptor_info *di = NULL;
	rtr_fgetc_t real_fgetc;
	rtr_fileno_t real_fileno;

	real_fgetc	= RETRACE_GET_REAL(fgetc);
	real_fileno	= RETRACE_GET_REAL(fileno);

	r = real_fgetc(stream);

	if (get_tracing_enabled()) {
		int old_trace_state;

		old_trace_state = trace_disable();

		if (stream) {
			fd = real_fileno(stream);
			di = file_descriptor_get(fd);
		}

		if (di && di->location)
			trace_printf(1, "fgetc(%p) [to: \"%s\", return: '%c'(%d)]\n", stream, di->location, r, r);
		else
			trace_printf(1, "fgetc(%p) [return: '%c'(%d)])\n", stream, r, r);

		trace_restore(old_trace_state);
	}

	return r;
}

RETRACE_REPLACE(fgetc)

void RETRACE_IMPLEMENTATION(strmode)(int mode, char *bp)
{
	rtr_strmode_t real_strmode;

	real_strmode = RETRACE_GET_REAL(strmode);

	real_strmode(mode, bp);

	trace_printf(1, "strmode(%d, \"%s\");\n", mode, bp);
}

RETRACE_REPLACE(strmode)
