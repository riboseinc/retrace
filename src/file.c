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
#include "malloc.h"
#include "strinject.h"
#include "printf.h"

#include "str.h"

#ifdef __FreeBSD__
#undef fileno
#endif

int RETRACE_IMPLEMENTATION(stat)(const char *path, struct stat *buf)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types_short[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void const *parameter_values_short[] = {&path, &buf};

	unsigned int parameter_types_full[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRUCT_STAT, PARAMETER_TYPE_END};
	void const *parameter_values_full[] = {&path, &buf};

	int r;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "stat";
	event_info.parameter_types = parameter_types_short;
	event_info.parameter_values = (void **) parameter_values_short;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	r = real_stat(path, buf);

	if (r == 0) {
		event_info.parameter_types = parameter_types_full;
		event_info.parameter_values = (void **) parameter_values_full;
	} else
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(stat, int, (const char *path, struct stat *buf), (path, buf))

int RETRACE_IMPLEMENTATION(chmod)(const char *path, mode_t mode)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_PERM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&path, &mode};
	int r;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "chmod";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_chmod(path, mode);
	if (r < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return (r);
}

RETRACE_REPLACE(chmod, int, (const char *path, mode_t mode), (path, mode))

int RETRACE_IMPLEMENTATION(fchmod)(int fd, mode_t mode)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_PERM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fd, &mode};
	int r;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fchmod";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_fchmod(fd, mode);
	if (r < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

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
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &fd;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
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
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_fseek(stream, offset, whence);
	if (r < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

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
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_fclose(stream);
	if (r != 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

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
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_FILE_STREAM;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	if (get_tracing_enabled() && file) {
		RTR_CONFIG_HANDLE config = RTR_CONFIG_START;

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
				event_info.logging_level |= RTR_LOG_LEVEL_REDIRECT;

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
			fd, FILE_DESCRIPTOR_TYPE_FILE, file);
	} else
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

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

	struct descriptor_info *di;
	int func_group = RTR_FUNC_GRP_FILE;

	/* get file descriptor info */
	di = file_descriptor_get(fd);
	if (di && di->type == FILE_DESCRIPTOR_TYPE_SOCK)
		func_group = RTR_FUNC_GRP_NET;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "close";
	event_info.function_group = func_group;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_close(fd);
	if (r < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

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
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_FILE_DESCRIPTOR;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_dup(oldfd);
	if (r < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

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
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_dup2(oldfd, newfd);
	if (r < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

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
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &old_mask;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
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
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_mkfifo(pathname, mode);
	if (ret < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

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
	mode_t mode = 0;

	if (flags & MODEFLAGS)
		mode = va_arg(ap, mode_t);

	return (real_open(pathname, flags, mode));
}

int RETRACE_IMPLEMENTATION(open)(const char *pathname, int flags, ...)
{
	int r;
	mode_t mode = 0;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_INT, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&pathname, &flags, &mode};
	va_list ap;

	if (flags & MODEFLAGS) {
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "open";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_open(pathname, flags, mode);

	if (r < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	else
		file_descriptor_update(
			r, FILE_DESCRIPTOR_TYPE_FILE, pathname);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE_V(open, int, (const char *pathname, int flags, ...), flags, open_v, (pathname, flags, ap))

size_t RETRACE_IMPLEMENTATION(fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_MEM_BUFFER_ARRAY, PARAMETER_TYPE_INT, PARAMETER_TYPE_INT, PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&size, &nmemb, &ptr, &size, &nmemb, &stream};
	int r;
	struct descriptor_info *di = NULL;

	void *inject_buffer = NULL;
	size_t inject_len;

	int enable_inject = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fwrite";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	if (rtr_str_inject(STRINJECT_FUNC_FWRITE, ptr, size * nmemb, &inject_buffer, &inject_len)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		inject_len /= size;
		parameter_values[2] = &inject_buffer;
		parameter_values[4] = &inject_len;

		enable_inject = 1;
	}

	retrace_log_and_redirect_before(&event_info);

	r = real_fwrite(enable_inject ? inject_buffer : ptr,
			size,
			enable_inject ? inject_len : nmemb,
			stream);
	if (r < (enable_inject ? inject_len : nmemb))
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	if (enable_inject)
		real_free(inject_buffer);

	return r;
}

RETRACE_REPLACE(fwrite, size_t, (const void *ptr, size_t size, size_t nmemb, FILE *stream), (ptr, size, nmemb, stream))

size_t RETRACE_IMPLEMENTATION(fread)(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_MEM_BUFFER_ARRAY, PARAMETER_TYPE_INT, PARAMETER_TYPE_INT, PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&size, &nmemb, &ptr, &size, &nmemb, &stream};
	int r;

	void *inject_buffer;
	size_t inject_len;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fread";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_fread(ptr, size, nmemb, stream);

	parameter_values[1] = &r;
	if (r < nmemb)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	if (rtr_str_inject(STRINJECT_FUNC_FREAD, ptr, r * size, &inject_buffer, &inject_len)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		r = inject_len / size;
		if (r > nmemb)
			r = nmemb;

		real_memcpy(ptr, inject_buffer, r * size);

		parameter_values[1] = &r;
		real_free(inject_buffer);
	}

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
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_fputc(c, stream);
	if (r == EOF)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

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
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_fputs(s, stream);
	if (r == EOF)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(fputs, int, (const char *s, FILE *stream), (s, stream))

char *RETRACE_IMPLEMENTATION(fgets)(char *s, int size, FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_INT, PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&s, &size, &stream};
	char *r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fgets";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_fgets(s, size, stream);
	if (!r && !feof(stream))
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(fgets, char *, (char *s, int size, FILE *stream), (s, size, stream))

int RETRACE_IMPLEMENTATION(fgetc)(FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&stream};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fgetc";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_fgetc(stream);
	if (r == EOF)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

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
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	real_strmode(mode, bp);

	retrace_log_and_redirect_after(&event_info);
}

RETRACE_REPLACE(strmode, void, (int mode, char *bp), (mode, bp))

static int
fcntl_v(int fildes, int cmd, va_list ap)
{
	struct descriptor_info *di;
	const char *old_location = "fcntl";
	int r;

	switch (cmd) {
	// hack for duplicating FD
	case F_DUPFD:
#if HAVE_DECL_F_DUPFD_CLOEXEC
	case F_DUPFD_CLOEXEC:
#endif
		r = real_fcntl(fildes, cmd, va_arg(ap, int));
		if (r >= 0) {
			di = file_descriptor_get(fildes);
			if (di->location != NULL)
				old_location = di->location;

			file_descriptor_update(r, di->type, old_location);
		}
		break;

	// (int) arg
	case F_SETFD:
		/* fallthrough */
	case F_SETFL:
		/* fallthrough */
#if HAVE_DECL_F_SETOWN
	case F_SETOWN:
		/* fallthrough */
#endif
#if HAVE_DECL_F_RDAHEAD
	case F_RDAHEAD:
		/* fallthrough */
#endif
#if HAVE_DECL_F_NOCACHE
	case F_NOCACHE:
		/* fallthrough */
#endif
#if HAVE_DECL_F_SETNOSIGPIPE
	case F_SETNOSIGPIPE:
		/* fallthrough */
#endif
#if HAVE_DECL_F_SETSIG
	case F_SETSIG:
		/* fallthrough */
#endif
#if HAVE_DECL_F_SETLEASE
	case F_SETLEASE:
		/* fallthrough */
#endif
#if HAVE_DECL_F_NOTIFY
	case F_NOTIFY:
		/* fallthrough */
#endif
#if HAVE_DECL_F_SETPIPE_SZ
	case F_SETPIPE_SZ:
		/* fallthrough */
#endif
#if HAVE_DECL_F_ADD_SEALS
	case F_ADD_SEALS:
#endif
		r = real_fcntl(fildes, cmd, va_arg(ap, int));
		break;

	// (void) arg
	case F_GETFD:
		/* fallthrough */
	case F_GETFL:
		/* fallthrough */
#if HAVE_DECL_F_GETOWN
	case F_GETOWN:
		/* fallthrough */
#endif
#if HAVE_DECL_F_SETSIZE
	case F_SETSIZE:
		/* fallthrough */
#endif
#if HAVE_DECL_F_FULLFSYNC
	case F_FULLFSYNC:
		/* fallthrough */
#endif
#if HAVE_DECL_F_GETNOSIGPIPE
	case F_GETNOSIGPIPE:
		/* fallthrough */
#endif
#if HAVE_DECL_F_GETSIG
	case F_GETSIG:
		/* fallthrough */
#endif
#if HAVE_DECL_F_GETLEASE
	case F_GETLEASE:
		/* fallthrough */
#endif
#if HAVE_DECL_F_GETPIPE_SZ
	case F_GETPIPE_SZ:
		/* fallthrough */
#endif
#if HAVE_DECL_F_GET_SEALS
	case F_GET_SEALS:
#endif
		r = real_fcntl(fildes, cmd);
		break;
	default:
		r = real_fcntl(fildes, cmd, va_arg(ap, void *));
	}

	return r;
}

int RETRACE_IMPLEMENTATION(fcntl)(int fildes, int cmd, ...)
{
	va_list ap;
	char extra_info_buf[128];

	int r;
	int int_parameter = 0;
#if HAVE_STRUCT_FLOCK
	struct flock *flock_parameter = NULL;
#endif
	void *maybe_parameter = NULL;
#if HAVE_DECL_F_GETPATH
	void *char_parameter = NULL;
#endif
#if HAVE_STRUCT_FSTORE && HAVE_DECL_F_PREALLOCATE
	struct fstore *fstore_parameter = NULL;
#endif
#if HAVE_STRUCT_FPUNCHHOLE && HAVE_DECL_F_PUNCHHOLE
	struct fpunchhole *fpunchhole_parameter = NULL;
#endif
#if HAVE_STRUCT_RADVISORY && HAVE_DECL_F_RDADVISE
	struct radvisory *radvisory_parameter = NULL;
#endif
#if HAVE_STRUCT_FBOOTSTRAPTRANSFER && (HAVE_DECL_F_READBOOTSTRAP || HAVE_DECL_F_WRITEBOOTSTRAP)
	struct fbootstraptransfer *fbootstraptransfer_parameter = NULL;
#endif
#if HAVE_STRUCT_LOG2PHYS && (HAVE_DECL_F_LOG2PHYS || HAVE_DECL_F_LOG2PHYS_EXT)
	struct log2phys *log2phys_parameter = NULL;
#endif
#if HAVE_DECL_F_GETOWN_EX && (HAVE_DECL_F_GETOWN_EX || HAVE_DECL_F_SETOWN_EX)
	struct f_owner_ex *f_owner_ex_parameter = NULL;
#endif

	struct rtr_event_info event_info;

	unsigned int parameter_types_maybe[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT, PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void const *parameter_values_maybe[] = {&fildes, &cmd, &maybe_parameter};

	unsigned int parameter_types_int[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values_int[] = {&fildes, &cmd, &int_parameter};

	unsigned int parameter_types_void[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values_void[] = {&fildes, &cmd};

#if HAVE_STRUCT_FLOCK
	unsigned int parameter_types_flock[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT, PARAMETER_TYPE_STRUCT_FLOCK, PARAMETER_TYPE_END};
	void const *parameter_values_flock[] = {&fildes, &cmd, &flock_parameter};
#endif
#if HAVE_DECL_F_GETPATH
	unsigned int parameter_types_char[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT, PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values_char[] = {&fildes, &cmd, &char_parameter};
#endif
#if HAVE_STRUCT_FSTORE && HAVE_DECL_F_PREALLOCATE
	unsigned int parameter_types_fstore[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT, PARAMETER_TYPE_STRUCT_FSTORE, PARAMETER_TYPE_END};
	void const *parameter_values_fstore[] = {&fildes, &cmd, &fstore_parameter};
#endif
#if HAVE_STRUCT_FPUNCHHOLE && HAVE_DECL_F_PUNCHHOLE
	unsigned int parameter_types_fpunchhole[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT, PARAMETER_TYPE_STRUCT_FPUNCHHOLE, PARAMETER_TYPE_END};
	void const *parameter_values_fpunchhole[] = {&fildes, &cmd, &fpunchhole_parameter};
#endif
#if HAVE_STRUCT_RADVISORY && HAVE_DECL_F_RDADVISE
	unsigned int parameter_types_radvisory[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT, PARAMETER_TYPE_STRUCT_RADVISORY, PARAMETER_TYPE_END};
	void const *parameter_values_radvisory[] = {&fildes, &cmd, &radvisory_parameter};
#endif
#if HAVE_STRUCT_FBOOTSTRAPTRANSFER && (HAVE_DECL_F_READBOOTSTRAP || HAVE_DECL_F_WRITEBOOTSTRAP)
	unsigned int parameter_types_fbootstraptransfer[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT, PARAMETER_TYPE_STRUCT_FBOOTSTRAPTRANSFER, PARAMETER_TYPE_END};
	void const *parameter_values_fbootstraptransfer[] = {&fildes, &cmd, &fbootstraptransfer_parameter};
#endif
#if HAVE_STRUCT_LOG2PHYS && (HAVE_DECL_F_LOG2PHYS || HAVE_DECL_F_LOG2PHYS_EXT)
	unsigned int parameter_types_log2phys[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT, PARAMETER_TYPE_STRUCT_LOG2PHYS, PARAMETER_TYPE_END};
	void const *parameter_values_log2phys[] = {&fildes, &cmd, &log2phys_parameter};
#endif
#if HAVE_DECL_F_GETOWN_EX && (HAVE_DECL_F_GETOWN_EX || HAVE_DECL_F_SETOWN_EX)
	unsigned int parameter_types_f_owner_ex[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT, PARAMETER_TYPE_STRUCT_F_GETOWN_EX, PARAMETER_TYPE_END};
	void const *parameter_values_f_owner_ex[] = {&fildes, &cmd, &f_owner_ex_parameter};
#endif

	struct descriptor_info *di;
	int func_group = RTR_FUNC_GRP_FILE;

	/* check if file descriptor is for socket */
	di = file_descriptor_get(fildes);
	if (di && di->type == FILE_DESCRIPTOR_TYPE_SOCK)
		func_group = RTR_FUNC_GRP_NET;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fcntl";
	event_info.function_group = func_group;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	va_start(ap, cmd);

	switch (cmd) {
	// (int) arg
	case F_DUPFD:
#if HAVE_DECL_F_DUPFD_CLOEXEC
	case F_DUPFD_CLOEXEC:
#endif
	case F_SETFD:
	case F_SETFL:
#if HAVE_DECL_F_SETOWN
	case F_SETOWN:
#endif
#if HAVE_DECL_F_RDAHEAD
	case F_RDAHEAD:
#endif
#if HAVE_DECL_F_NOCACHE
	case F_NOCACHE:
#endif
#if HAVE_DECL_F_SETNOSIGPIPE
	case F_SETNOSIGPIPE:
#endif
#if HAVE_DECL_F_SETSIG
	case F_SETSIG:
#endif
#if HAVE_DECL_F_SETLEASE
	case F_SETLEASE:
#endif
#if HAVE_DECL_F_NOTIFY
	case F_NOTIFY:
#endif
#if HAVE_DECL_F_SETPIPE_SZ
	case F_SETPIPE_SZ:
#endif
#if HAVE_DECL_F_ADD_SEALS
	case F_ADD_SEALS:
#endif
		event_info.parameter_types = parameter_types_int;
		event_info.parameter_values = (void **) parameter_values_int;

		int_parameter = va_arg(ap, int);

		break;

	// (void) arg
	case F_GETFD:
	case F_GETFL:
#if HAVE_DECL_F_GETOWN
	case F_GETOWN:
#endif
#if HAVE_DECL_F_SETSIZE
	case F_SETSIZE:
#endif
#if HAVE_DECL_F_FULLFSYNC
	case F_FULLFSYNC:
#endif
#if HAVE_DECL_F_GETNOSIGPIPE
	case F_GETNOSIGPIPE:
#endif
#if HAVE_DECL_F_GETSIG
	case F_GETSIG:
#endif
#if HAVE_DECL_F_GETLEASE
	case F_GETLEASE:
#endif
#if HAVE_DECL_F_GETPIPE_SZ
	case F_GETPIPE_SZ:
#endif
#if HAVE_DECL_F_GET_SEALS
	case F_GET_SEALS:
#endif
		event_info.parameter_types = parameter_types_void;
		event_info.parameter_values = (void **) parameter_values_void;

		break;

#if HAVE_STRUCT_FLOCK
	// (struct flock *) arg
	case F_SETLK:
#if HAVE_DECL_F_SETLKW
	case F_SETLKW:
#endif
	case F_GETLK:
#if HAVE_DECL_F_OFD_SETLK
	case F_OFD_SETLK:
#endif
#if HAVE_DECL_F_OFD_SETLKW
	case F_OFD_SETLKW:
#endif
#if HAVE_DECL_F_OFD_GETLK
	case F_OFD_GETLK:
#endif
		event_info.parameter_types = parameter_types_flock;
		event_info.parameter_values = (void **) parameter_values_flock;

		flock_parameter = va_arg(ap, struct flock *);

		break;
#endif

#if HAVE_DECL_F_GETPATH
	// char* arg
	case F_GETPATH:
		event_info.parameter_types = parameter_types_char;
		event_info.parameter_values = (void **) parameter_values_char;

		char_parameter = va_arg(ap, char*);

		break;
#endif
#if HAVE_STRUCT_FSTORE && HAVE_DECL_F_PREALLOCATE
	// (struct fstore *) arg
	case F_PREALLOCATE:
		event_info.parameter_types = parameter_types_fstore;
		event_info.parameter_values = (void **) parameter_values_fstore;

		fstore_parameter = va_arg(ap, struct fstore *);

		break;
#endif
#if HAVE_STRUCT_FPUNCHHOLE && HAVE_DECL_F_PUNCHHOLE
	// (struct fpunchhole *) arg
	case F_PUNCHHOLE:
		event_info.parameter_types = parameter_types_fpunchhole;
		event_info.parameter_values = (void **) parameter_values_fpunchhole;

		fpunchhole_parameter = va_arg(ap, struct fpunchhole *);

		break;
#endif
#if HAVE_STRUCT_RADVISORY && HAVE_DECL_F_RDADVISE
	// (struct radvisory *) arg
	case F_RDADVISE:
		event_info.parameter_types = parameter_types_radvisory;
		event_info.parameter_values = (void **) parameter_values_radvisory;

		radvisory_parameter = va_arg(ap, struct radvisory *);

		break;
#endif
#if HAVE_STRUCT_FBOOTSTRAPTRANSFER && (HAVE_DECL_F_READBOOTSTRAP || HAVE_DECL_F_WRITEBOOTSTRAP)
	// (struct fbootstraptransfer *) arg
	case F_READBOOTSTRAP:
	case F_WRITEBOOTSTRAP:
		event_info.parameter_types = parameter_types_fbootstraptransfer;
		event_info.parameter_values = (void **) parameter_values_fbootstraptransfer;

		fbootstraptransfer_parameter = va_arg(ap, struct fbootstraptransfer *);

		break;
#endif
#if HAVE_STRUCT_LOG2PHYS && (HAVE_DECL_F_LOG2PHYS || HAVE_DECL_F_LOG2PHYS_EXT)
	// (struct log2phys *) arg
	case F_LOG2PHYS:
	case F_LOG2PHYS_EXT:
		event_info.parameter_types = parameter_types_log2phys;
		event_info.parameter_values = (void **) parameter_values_log2phys;

		log2phys_parameter = va_arg(ap, struct log2phys *);

		break;
#endif
#if HAVE_DECL_F_GETOWN_EX && (HAVE_DECL_F_GETOWN_EX || HAVE_DECL_F_SETOWN_EX)
	// (struct f_owner_ex *) arg
	case F_GETOWN_EX:
	case F_SETOWN_EX:
		event_info.parameter_types = parameter_types_f_owner_ex;
		event_info.parameter_values = (void **) parameter_values_f_owner_ex;

		f_owner_ex_parameter = va_arg(ap, struct f_owner_ex *);

		break;
#endif
	default:
		event_info.parameter_types = parameter_types_maybe;
		event_info.parameter_values = (void **) parameter_values_maybe;
		real_sprintf(extra_info_buf, "Unsupported fcntl command: %d", cmd);
		event_info.extra_info = extra_info_buf;

		maybe_parameter = va_arg(ap, void *);
	}
	va_end(ap);

	retrace_log_and_redirect_before(&event_info);

	va_start(ap, cmd);
	r = fcntl_v(fildes, cmd, ap);
	va_end(ap);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE_V(fcntl, int, (int fildes, int cmd, ...), cmd, fcntl_v, (fildes, cmd, ap))

int RETRACE_IMPLEMENTATION(fgetpos)(FILE *stream, fpos_t *pos)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void *parameter_values[] = {&stream, &pos};
	int ret;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fgetpos";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_fgetpos(stream, pos);
	if (ret < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(fgetpos, int, (FILE *stream, fpos_t *pos), (stream, pos))


int RETRACE_IMPLEMENTATION(fsetpos)(FILE *stream, const fpos_t *pos)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void *parameter_values[] = {&stream, &pos};
	int ret;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "fsetpos";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_fsetpos(stream, pos);
	if (ret < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(fsetpos, int, (FILE *stream, const fpos_t *pos), (stream, pos))

long RETRACE_IMPLEMENTATION(ftell)(FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void *parameter_values[] = {&stream};
	long ret;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "ftell";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_ftell(stream);
	if (ret < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(ftell, long, (FILE *stream), (stream))


void RETRACE_IMPLEMENTATION(rewind)(FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void *parameter_values[] = {&stream};

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "rewind";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	real_rewind(stream);

	retrace_log_and_redirect_after(&event_info);
}

RETRACE_REPLACE(rewind, void, (FILE *stream), (stream))


int RETRACE_IMPLEMENTATION(feof)(FILE *stream)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_STREAM, PARAMETER_TYPE_END};
	void *parameter_values[] = {&stream};
	int ret;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "feof";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_feof(stream);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(feof, int, (FILE *stream), (stream))

