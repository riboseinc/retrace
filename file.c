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
	char *perm_p = &perm[0];
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_INT_OCTAL | PARAMETER_FLAG_STRING_NEXT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&path, &mode, &perm_p};
	int r;

	trace_mode(mode, perm);


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "chmod";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_chmod(path, mode);

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(chmod, int, (const char *path, mode_t mode), (path, mode))

int RETRACE_IMPLEMENTATION(fchmod)(int fd, mode_t mode)
{
	char perm[10];
	char *perm_p = &perm[0];
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT_OCTAL | PARAMETER_FLAG_STRING_NEXT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fd, &mode, &perm_p};
	int r;

	trace_mode(mode, perm);


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fchmod";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_fchmod(fd, mode);

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(fchmod, int, (int fd, mode_t mode), (fd, mode))

int RETRACE_IMPLEMENTATION(fileno)(FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&stream};
	int fd;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fileno";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &fd;
	retrace_log_and_redirect_before(&event_info);

	fd = real_fileno(stream);

	retrace_log_and_redirect_after(&event_info);

	return real_fileno(stream);
}

RETRACE_REPLACE(fileno, int, (FILE *stream), (stream))

int RETRACE_IMPLEMENTATION(fseek)(FILE *stream, long offset, int whence)
{
	char *operation = NULL;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_INT, PARAMETER_TYPE_INT | PARAMETER_FLAG_STRING_NEXT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&stream, &offset, &whence, &operation};
	int r;

	if (whence == 0)
		operation = "SEEK_SET";
	else if (whence == 1)
		operation = "SEEK_CUR";
	else if (whence == 2)
		operation = "SEEK_END";
	else
		operation = "UNDEFINED";


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fseek";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_fseek(stream, offset, whence);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(fseek, int, (FILE *stream, long offset, int whence), (stream, offset, whence))

int RETRACE_IMPLEMENTATION(fclose)(FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&stream};
	int r;
	int fd = -1;

	if (stream)
		fd = real_fileno(stream);


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fclose";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_fclose(stream);

	retrace_log_and_redirect_after(&event_info);

	if (fd > 0)
		file_descriptor_remove(fd);

	return r;
}

RETRACE_REPLACE(fclose, int, (FILE *stream), (stream))

FILE *RETRACE_IMPLEMENTATION(fopen)(const char *file, const char *mode)
{
	int did_redirect = 0;
	int fd = 0;
	char *match_file = NULL;
	FILE *ret;
	char *redirect_file = NULL;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&file, &mode};


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fopen";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_FILE_STREAM;
	event_info.return_value = &ret;
	retrace_log_and_redirect_before(&event_info);

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

	if (ret) {
		int fd;

		fd = real_fileno(ret);

		file_descriptor_update(
			fd, FILE_DESCRIPTOR_TYPE_FILE, file, 0);
	}

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(fopen, FILE *, (const char *file, const char *mode), (file, mode))

int RETRACE_IMPLEMENTATION(close)(int fd)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fd};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "close";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_close(fd);

	retrace_log_and_redirect_after(&event_info);

	file_descriptor_remove(fd);

	return (r);
}

RETRACE_REPLACE(close, int, (int fd), (fd))

int RETRACE_IMPLEMENTATION(dup)(int oldfd)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&oldfd};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "dup";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_FILE_DESCRIPTOR;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_dup(oldfd);

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(dup, int, (int oldfd), (oldfd))

int RETRACE_IMPLEMENTATION(dup2)(int oldfd, int newfd)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&oldfd, &newfd};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "dup2";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_dup2(oldfd, newfd);

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(dup2, int, (int oldfd, int newfd), (oldfd, newfd))

mode_t RETRACE_IMPLEMENTATION(umask)(mode_t mask)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT,  PARAMETER_TYPE_END};
	void const *parameter_values[] = {&mask};
	mode_t old_mask;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "umask";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &old_mask;
	retrace_log_and_redirect_before(&event_info);

	old_mask = real_umask(mask);

	retrace_log_and_redirect_after(&event_info);

	return old_mask;
}

RETRACE_REPLACE(umask, mode_t, (mode_t mask), (mask))

int RETRACE_IMPLEMENTATION(mkfifo)(const char *pathname, mode_t mode)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&pathname, &mode};
	int ret;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "mkfifo";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	retrace_log_and_redirect_before(&event_info);

	ret = real_mkfifo(pathname, mode);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(mkfifo, int, (const char *pathname, mode_t mode), (pathname, mode))

#ifdef __APPLE__
#define MODEFLAGS O_CREAT
#else
#ifdef O_TMPFILE
#define MODEFLAGS (O_CREAT | O_TMPFILE)
#else
#define MODEFLAGS O_CREAT
#endif
#endif

static int
open_v(const char *pathname, int flags, va_list ap)
{
	int r;
	mode_t mode = 0;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_INT, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&pathname, &flags, &mode};

	if (flags & MODEFLAGS)
		mode = va_arg(ap, int);


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "open";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);


	if (flags & MODEFLAGS)
		r = real_open(pathname, flags, mode);
	else
		r =  real_open(pathname, flags);

	if (r > 0) {
		file_descriptor_update(
			r, FILE_DESCRIPTOR_TYPE_FILE, pathname, 0);
	}

	retrace_log_and_redirect_after(&event_info);

	return r;
}

int RETRACE_IMPLEMENTATION(open)(const char *pathname, int flags, ...)
{
	int fd;
	va_list ap;

	va_start(ap, flags);
	fd = open_v(pathname, flags, ap);
	va_end(ap);

	return fd;
}

RETRACE_REPLACE_V(open, int, (const char *pathname, int flags, ...), flags, open_v, (pathname, flags, ap))

size_t RETRACE_IMPLEMENTATION(fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_MEM_BUFFER_ARRAY, PARAMETER_TYPE_INT, PARAMETER_TYPE_INT, PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&size, &nmemb, &ptr, &size, &nmemb, &stream};
	int r;
	struct descriptor_info *di = NULL;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fwrite";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_fwrite(ptr, size, nmemb, stream);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(fwrite, size_t, (const void *ptr, size_t size, size_t nmemb, FILE *stream), (ptr, size, nmemb, stream))

size_t RETRACE_IMPLEMENTATION(fread)(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_MEM_BUFFER_ARRAY, PARAMETER_TYPE_INT, PARAMETER_TYPE_INT, PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&size, &nmemb, &ptr, &size, &nmemb, &stream};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fread";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_fread(ptr, size, nmemb, stream);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(fread, size_t, (void *ptr, size_t size, size_t nmemb, FILE *stream), (ptr, size, nmemb, stream))

int RETRACE_IMPLEMENTATION(fputc)(int c, FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_CHAR, PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&c, &stream};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fputc";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_fputc(c, stream);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(fputc, int, (int c, FILE *stream), (c, stream))

int RETRACE_IMPLEMENTATION(fputs)(const char *s, FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&s, &stream};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fputs";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_fputs(s, stream);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(fputs, int, (const char *s, FILE *stream), (s, stream))

int RETRACE_IMPLEMENTATION(fgetc)(FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&stream};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fgetc";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_fgetc(stream);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(fgetc, int, (FILE *stream), (stream))

void RETRACE_IMPLEMENTATION(strmode)(int mode, char *bp)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&mode, &bp};


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "strmode";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;
	retrace_log_and_redirect_before(&event_info);

	real_strmode(mode, bp);

	retrace_log_and_redirect_after(&event_info);
}

RETRACE_REPLACE(strmode, void, (int mode, char *bp), (mode, bp))
