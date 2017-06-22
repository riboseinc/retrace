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
	char perm[10];
	int r;

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

RETRACE_REPLACE(stat, int, (const char *path, struct stat *buf), (path, buf))

int RETRACE_IMPLEMENTATION(chmod)(const char *path, mode_t mode)
{
	char perm[10];

	trace_mode(mode, perm);

	trace_printf(1, "chmod(\"%s\", %o); [%s]\n", path, mode, perm);

	return real_chmod(path, mode);
}

RETRACE_REPLACE(chmod, int, (const char *path, mode_t mode), (path, mode))

int RETRACE_IMPLEMENTATION(fchmod)(int fd, mode_t mode)
{
	char perm[10];

	trace_mode(mode, perm);

	trace_printf(1, "fchmod(%d, %o); [%s]\n", fd, mode, perm);

	return real_fchmod(fd, mode);
}

RETRACE_REPLACE(fchmod, int, (int fd, mode_t mode), (fd, mode))

int RETRACE_IMPLEMENTATION(fileno)(FILE *stream)
{
	int fd;

	fd = real_fileno(stream);

	trace_printf(1, "fileno(%d);\n", fd);

	return real_fileno(stream);
}

RETRACE_REPLACE(fileno, int, (FILE *stream), (stream))

int RETRACE_IMPLEMENTATION(fseek)(FILE *stream, long offset, int whence)
{
	int fd;

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

RETRACE_REPLACE(fseek, int, (FILE *stream, long offset, int whence), (stream, offset, whence))

int RETRACE_IMPLEMENTATION(fclose)(FILE *stream)
{
	int fd;
	struct descriptor_info *di;

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

RETRACE_REPLACE(fclose, int, (FILE *stream), (stream))

FILE *RETRACE_IMPLEMENTATION(fopen)(const char *file, const char *mode)
{
	int did_redirect = 0;
	int fd = 0;
	char *match_file = NULL;
	FILE *ret;
	char *redirect_file = NULL;

	if (get_tracing_enabled() && file) {
		RTR_CONFIG_HANDLE config = NULL;

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

				break;
			}
		}
	}

	if (!did_redirect)
		ret = real_fopen(file, mode);

	trace_printf(1, "fopen(\"%s%s\", \"%s\"); [%d]\n",
	    did_redirect ? redirect_file : file,
	    did_redirect ? " [redirected]" : "",
	    mode, fd);

	return ret;
}

RETRACE_REPLACE(fopen, FILE *, (const char *file, const char *mode), (file, mode))

int RETRACE_IMPLEMENTATION(close)(int fd)
{
	struct descriptor_info *di;

	di = file_descriptor_get(fd);
	if (di && di->location)
		trace_printf(1, "close(%d) [was pointing to %s];\n", fd, di->location);
	else
		trace_printf(1, "close(%d);\n", fd);

	file_descriptor_remove(fd);

	return real_close(fd);
}

RETRACE_REPLACE(close, int, (int fd), (fd))

int RETRACE_IMPLEMENTATION(dup)(int oldfd)
{
	trace_printf(1, "dup(%d)\n", oldfd);

	return real_dup(oldfd);
}

RETRACE_REPLACE(dup, int, (int oldfd), (oldfd))

int RETRACE_IMPLEMENTATION(dup2)(int oldfd, int newfd)
{
	trace_printf(1, "dup2(%d, %d)\n", oldfd, newfd);

	return real_dup2(oldfd, newfd);
}

RETRACE_REPLACE(dup2, int, (int oldfd, int newfd), (oldfd, newfd))

mode_t RETRACE_IMPLEMENTATION(umask)(mode_t mask)
{
	mode_t old_mask;

	old_mask = real_umask(mask);

	trace_printf(1, "umask(%d); [%d]\n", mask, old_mask);

	return old_mask;
}

RETRACE_REPLACE(umask, mode_t, (mode_t mask), (mask))

int RETRACE_IMPLEMENTATION(mkfifo)(const char *pathname, mode_t mode)
{
	int ret;

	ret = real_mkfifo(pathname, mode);

	trace_printf(1, "mkfifo(%s, %d); [%d]\n", pathname, mode, ret);

	return ret;
}

RETRACE_REPLACE(mkfifo, int, (const char *pathname, mode_t mode), (pathname, mode))

typedef int (*rtr_open_mode_t)(const char *pathname, int flags, mode_t mode);
typedef int (*rtr_open_nomode_t)(const char *pathname, int flags);

#ifdef __APPLE__
#define MODEFLAGS O_CREAT
#else
#define MODEFLAGS (O_CREAT | O_TMPFILE)
#endif

static int
open_v(const char *pathname, int flags, va_list ap)
{
	if (flags & MODEFLAGS)
		return real_open(pathname, flags, va_arg(ap, int));
	return real_open(pathname, flags);
}

int RETRACE_IMPLEMENTATION(open)(const char *pathname, int flags, ...)
{
	int fd;
	va_list ap;

	va_start(ap, flags);
	fd = open_v(pathname, flags, ap);
	va_end(ap);

	if (flags & MODEFLAGS) {
		va_start(ap, flags);
		trace_printf(1, "open(%s, %u, %u) [return: fd]\n", pathname,
		    flags, va_arg(ap, int), fd);
		va_end(ap);
	} else
		trace_printf(1, "open(%s, %u) [return: fd]\n", pathname, flags, fd);

	if (fd > 0) {
		file_descriptor_update(
				fd, FILE_DESCRIPTOR_TYPE_FILE, pathname, 0);
	}

	return fd;
}

RETRACE_REPLACE_V(open, int, (const char *pathname, int flags, ...), flags, open_v, (pathname, flags, ap))

size_t RETRACE_IMPLEMENTATION(fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t i;
	int r, fd;
	struct descriptor_info *di = NULL;

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

RETRACE_REPLACE(fwrite, size_t, (const void *ptr, size_t size, size_t nmemb, FILE *stream), (ptr, size, nmemb, stream))

size_t RETRACE_IMPLEMENTATION(fread)(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	int i, r, fd;
	struct descriptor_info *di = NULL;

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

RETRACE_REPLACE(fread, size_t, (void *ptr, size_t size, size_t nmemb, FILE *stream), (ptr, size, nmemb, stream))

int RETRACE_IMPLEMENTATION(fputc)(int c, FILE *stream)
{
	int fd;
	int r;
	struct descriptor_info *di = NULL;

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

RETRACE_REPLACE(fputc, int, (int c, FILE *stream), (c, stream))

int RETRACE_IMPLEMENTATION(fputs)(const char *s, FILE *stream)
{
	int r, fd;
	struct descriptor_info *di = NULL;

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

RETRACE_REPLACE(fputs, int, (const char *s, FILE *stream), (s, stream))

int RETRACE_IMPLEMENTATION(fgetc)(FILE *stream)
{
	int r;
	int fd;
	struct descriptor_info *di = NULL;

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

RETRACE_REPLACE(fgetc, int, (FILE *stream), (stream))

void RETRACE_IMPLEMENTATION(strmode)(int mode, char *bp)
{
	real_strmode(mode, bp);

	trace_printf(1, "strmode(%d, \"%s\");\n", mode, bp);
}

RETRACE_REPLACE(strmode, void, (int mode, char *bp), (mode, bp))
