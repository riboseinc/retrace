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
#include "dir.h"

DIR *RETRACE_IMPLEMENTATION(opendir)(const char *dirname)
{
	DIR *dirp;
	rtr_opendir_t real_opendir;
	rtr_dirfd_t real_dirfd;

	real_opendir	= RETRACE_GET_REAL(opendir);
	real_dirfd	= RETRACE_GET_REAL(dirfd);

	dirp = real_opendir(dirname);

	if (dirp)
		trace_printf(1, "opendir(\"%s\"); [%d]\n", dirname, real_dirfd(dirp));
	else
		trace_printf(1, "opendir(\"%s\"); NULL\n", dirname);

	return dirp;
}

RETRACE_REPLACE(opendir)

int RETRACE_IMPLEMENTATION(closedir)(DIR *dirp)
{
	rtr_closedir_t real_closedir;
	rtr_dirfd_t real_dirfd;

	real_closedir	= RETRACE_GET_REAL(closedir);
	real_dirfd	= RETRACE_GET_REAL(dirfd);

	trace_printf(1, "closedir(%d);\n", real_dirfd(dirp));

	return real_closedir(dirp);
}

RETRACE_REPLACE(closedir)

DIR *RETRACE_IMPLEMENTATION(fdopendir)(int fd)
{
	rtr_fdopendir_t real_fdopendir;

	real_fdopendir = RETRACE_GET_REAL(fdopendir);

	trace_printf(1, "fdopendir(%d)\n", fd);

	return real_fdopendir(fd);
}

RETRACE_REPLACE(fdopendir)

int RETRACE_IMPLEMENTATION(readdir_r)(DIR *dirp, struct dirent *entry, struct dirent **result)
{
	int dir_fd;
	int ret;
	rtr_readdir_r_t real_readdir_r;
	rtr_dirfd_t real_dirfd;

	real_readdir_r	= RETRACE_GET_REAL(readdir_r);
	real_dirfd	= RETRACE_GET_REAL(dirfd);

	/* get directory file descriptor */
	dir_fd = real_dirfd(dirp);

	ret = real_readdir_r(dirp, entry, result);
	if (ret == 0)
		if ((*result))
			trace_printf(1, "real_readdir_r(%d, , %s);\n", dir_fd, (*result)->d_name);
		else
			trace_printf(1, "real_readdir_r(%d, NULL);\n", dir_fd, entry->d_name);
	else
		trace_printf(1, "real_readdir_r(%d, , NULL); [err: %d]\n", dir_fd, ret);

	return ret;
}

RETRACE_REPLACE(readdir_r)

long RETRACE_IMPLEMENTATION(telldir)(DIR *dirp)
{
	int dir_fd;
	long offset;
	rtr_telldir_t real_telldir;
	rtr_dirfd_t real_dirfd;

	real_telldir	= RETRACE_GET_REAL(telldir);
	real_dirfd	= RETRACE_GET_REAL(dirfd);

	dir_fd = real_dirfd(dirp);
	offset = real_telldir(dirp);

	trace_printf(1, "telldir(%d); [%ld]\n", dir_fd, offset);

	return offset;
}

RETRACE_REPLACE(telldir)

void RETRACE_IMPLEMENTATION(seekdir)(DIR *dirp, long loc)
{
	int dir_fd;
	rtr_seekdir_t real_seekdir;
	rtr_dirfd_t real_dirfd;

	real_seekdir	= RETRACE_GET_REAL(seekdir);
	real_dirfd	= RETRACE_GET_REAL(dirfd);

	/* get dir fd */
	dir_fd = real_dirfd(dirp);
	real_seekdir(dirp, loc);

	trace_printf(1, "seekdir(%d, %ld);\n", dir_fd, loc);
}

RETRACE_REPLACE(seekdir)

void RETRACE_IMPLEMENTATION(rewinddir)(DIR *dirp)
{
	int dir_fd;
	rtr_rewinddir_t real_rewinddir;
	rtr_dirfd_t real_dirfd;

	real_rewinddir	= RETRACE_GET_REAL(rewinddir);
	real_dirfd	= RETRACE_GET_REAL(dirfd);

	/* get dir fd */
	dir_fd = real_dirfd(dirp);
	real_rewinddir(dirp);

	trace_printf(1, "rewinddir(%d);\n", dir_fd);
}

RETRACE_REPLACE(rewinddir)

int RETRACE_IMPLEMENTATION(dirfd)(DIR *dirp)
{
	int dir_fd;
	rtr_dirfd_t real_dirfd;

	real_dirfd = RETRACE_GET_REAL(dirfd);

	/* get dir fd */
	dir_fd = real_dirfd(dirp);

	trace_printf(1, "dirfd(), [%d];\n", dir_fd);

	return dir_fd;
}

RETRACE_REPLACE(dirfd)
