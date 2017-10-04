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

#include "../config.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "frontend.h"
#include "tracefd.h"
#include "handlers.h"

#if HAVE_DECL_O_TMPFILE
#define OPEN_CREATE_FLAGS (O_CREAT | O_TMPFILE)
#else
#define OPEN_CREATE_FLAGS O_CREAT
#endif

static const struct fdinfo *
_get_fdinfo(struct fdinfo_h *infos, enum tracefd_keytype type, pid_t pid,
	void *key)
{
	const struct fdinfo *fi;

	SLIST_FOREACH(fi, infos, next)
		if (fi->pid == pid && fi->type == type && fi->key == key)
			return fi;

	return NULL;
}

static void
_set_fdinfo(struct fdinfo_h *infos, enum tracefd_keytype type, pid_t pid,
	void *key, const char *info)
{
	struct fdinfo *fi;
	size_t infolen;

	fi = (struct fdinfo *)_get_fdinfo(infos, type, pid, key);
	if (fi != NULL) {
		SLIST_REMOVE(infos, fi, fdinfo, next);
		free(fi);
	}

	if (info == NULL)
		return;

	infolen = strlen(info) + 1;
	fi = malloc(sizeof(struct fdinfo) + infolen);
	if (fi != NULL) {
		fi->type = type;
		fi->key = key;
		fi->pid = pid;
		fi->info = (char *)&fi[1];
		strncpy(fi->info, info, infolen);
		SLIST_INSERT_HEAD(infos, fi, next);
	}
}

static void
inherit(struct retrace_endpoint *ep)
{
	struct handler_info *hi = ep->handle->user_data;
	struct fdinfo_h *infos = &hi->fdinfos, new_infos;
	struct fdinfo *fi;

	SLIST_INIT(&new_infos);

	SLIST_FOREACH(fi, infos, next) {
		if (fi->pid != ep->ppid)
			continue;
		_set_fdinfo(&new_infos, fi->type, ep->pid, fi->key, fi->info);
	}

	while (!SLIST_EMPTY(&new_infos)) {
		fi = SLIST_FIRST(&new_infos);
		SLIST_REMOVE_HEAD(&new_infos, next);
		SLIST_INSERT_HEAD(infos, fi, next);
	}
}

void
set_dirinfo(struct fdinfo_h *infos, pid_t pid, DIR *dir, const char *info)
{
	_set_fdinfo(infos, tracefd_keytype_dirp, pid, dir, info);
}

const struct fdinfo *
get_dirinfo(struct fdinfo_h *infos, pid_t pid, DIR *dir)
{
	return _get_fdinfo(infos, tracefd_keytype_dirp, pid, dir);
}

void
set_streaminfo(struct fdinfo_h *infos, pid_t pid, FILE *stream,
	const char *info)
{
	_set_fdinfo(infos, tracefd_keytype_stream, pid, stream, info);
}

const struct fdinfo *
get_streaminfo(struct fdinfo_h *infos, pid_t pid, FILE *stream)
{
	return _get_fdinfo(infos, tracefd_keytype_stream, pid, stream);
}

void
set_fdinfo(struct fdinfo_h *infos, pid_t pid, int fd, const char *info)
{
	_set_fdinfo(infos, tracefd_keytype_fd, pid, (void *)(long)fd, info);
}

const struct fdinfo *
get_fdinfo(struct fdinfo_h *infos, pid_t pid, int fd)
{
	return _get_fdinfo(infos, tracefd_keytype_fd, pid, (void *)(long)fd);
}

static void
opendir_postcall(struct retrace_endpoint *ep,
	struct retrace_call_context *context)
{
	struct retrace_opendir_params *params =
	    (struct retrace_opendir_params *)&context->params;
	struct handler_info *hi = ep->handle->user_data;
	struct fdinfo_h *fdinfos = &hi->fdinfos;
	DIR *dir = *(DIR **)context->result;
	int x;
	char name[1024], info[1024];

	if (dir == NULL)
		return;

	x = retrace_fetch_string(ep->fd, params->name, name, sizeof(name));
	if (x == 0)
		return;

	snprintf(info, sizeof(info), "opendir(\"%s\")", name);
	set_dirinfo(fdinfos, ep->pid, dir, info);
}

static void
fdopendir_postcall(struct retrace_endpoint *ep,
	struct retrace_call_context *context)
{
	struct retrace_fdopendir_params *params =
	    (struct retrace_fdopendir_params *)&context->params;
	struct handler_info *hi = ep->handle->user_data;
	struct fdinfo_h *fdinfos = &hi->fdinfos;
	DIR *dir = *(DIR **)context->result;
	char info[1024];
	const struct fdinfo *oldinfo;

	if (dir == NULL)
		return;

	oldinfo = get_fdinfo(fdinfos, ep->pid, params->fd);
	if (oldinfo != NULL)
		snprintf(info, sizeof(info), "fdopendir(%d:%s)",
		    params->fd, oldinfo->info);
	else
		snprintf(info, sizeof(info), "fdopendir(%d)",
		    params->fd);
	set_dirinfo(fdinfos, ep->pid, dir, info);
}

static void
fopen_postcall(struct retrace_endpoint *ep,
	struct retrace_call_context *context)
{
	struct retrace_fopen_params *params =
	    (struct retrace_fopen_params *)&context->params;
	struct handler_info *hi = ep->handle->user_data;
	struct fdinfo_h *fdinfos = &hi->fdinfos;
	FILE *s = *(FILE **)context->result;
	int x;
	char path[1024], mode[16], info[1024];

	if (s == NULL)
		return;

	x = retrace_fetch_string(ep->fd, params->path, path, sizeof(path));
	if (x == 0)
		return;

	x = retrace_fetch_string(ep->fd, params->mode, mode, sizeof(mode));
	if (x == 0)
		return;

	snprintf(info, sizeof(info), "fopen(\"%s\", \"%s\")", path, mode);
	set_streaminfo(fdinfos, ep->pid, s, info);
}

static void
creat_postcall(struct retrace_endpoint *ep,
	struct retrace_call_context *context)
{
	struct retrace_creat_params *params =
	    (struct retrace_creat_params *)&context->params;
	struct handler_info *hi = ep->handle->user_data;
	struct fdinfo_h *fdinfos = &hi->fdinfos;
	int fd = *(int *)context->result;
	int x;
	char path[1024], info[1024];

	if (fd == -1)
		return;

	x = retrace_fetch_string(ep->fd, params->pathname, path,
	    sizeof(path));
	if (x == 0)
		return;

	snprintf(info, sizeof(info), "creat(\"%s\")", path);
	set_fdinfo(fdinfos, ep->pid, fd, info);
}

static void
open_postcall(struct retrace_endpoint *ep,
	struct retrace_call_context *context)
{
	struct retrace_open_params *params =
	    (struct retrace_open_params *)&context->params;
	struct handler_info *hi = ep->handle->user_data;
	struct fdinfo_h *fdinfos = &hi->fdinfos;
	int fd = *(int *)context->result;
	int x;
	char path[1024], info[1024];

	if (fd == -1)
		return;

	x = retrace_fetch_string(ep->fd, params->pathname, path,
	    sizeof(path));
	if (x == 0)
		return;

	if (params->flags & OPEN_CREATE_FLAGS)
		snprintf(info, sizeof(info), "open(\"%s\", 0x%x, 0%.3o)",
		    path, params->flags, params->mode);
	else
		snprintf(info, sizeof(info), "open(\"%s\", 0x%x)",
		    path, params->flags);
	set_fdinfo(fdinfos, ep->pid, fd, info);
}

static void
openat_postcall(struct retrace_endpoint *ep,
	struct retrace_call_context *context)
{
	struct retrace_openat_params *params =
	    (struct retrace_openat_params *)&context->params;
	struct handler_info *hi = ep->handle->user_data;
	struct fdinfo_h *fdinfos = &hi->fdinfos;
	int fd = *(int *)context->result;
	int x;
	char path[1024], info[1024];

	if (fd == -1)
		return;

	x = retrace_fetch_string(ep->fd, params->pathname, path,
	    sizeof(path));
	if (x == 0)
		return;

	if (params->flags & OPEN_CREATE_FLAGS)
		snprintf(info, sizeof(info),
		    "openat(%d, \"%s\", 0x%x, 0%.3o)",
		    params->dirfd, path, params->flags, params->mode);
	else
		snprintf(info, sizeof(info), "openat(%d, \"%s\", 0x%x)",
		    params->dirfd, path, params->flags);

	set_fdinfo(fdinfos, ep->pid, fd, info);
}

static void
socket_postcall(struct retrace_endpoint *ep,
	struct retrace_call_context *context)
{
	struct retrace_socket_params *params =
	    (struct retrace_socket_params *)&context->params;
	struct handler_info *hi = ep->handle->user_data;
	struct fdinfo_h *fdinfos = &hi->fdinfos;
	int fd = *(int *)context->result;
	char info[1024];

	if (fd == -1)
		return;

	snprintf(info, sizeof(info), "socket(%d, %d, %d)", params->domain,
	    params->type, params->protocol);
	set_fdinfo(fdinfos, ep->pid, fd, info);
}

static void
socketpair_postcall(struct retrace_endpoint *ep,
	struct retrace_call_context *context)
{
	struct retrace_socketpair_params *params =
	    (struct retrace_socketpair_params *)&context->params;
	struct handler_info *hi = ep->handle->user_data;
	struct fdinfo_h *fdinfos = &hi->fdinfos;
	int result = *(int *)context->result;
	char info[1024];
	int sv[2];

	if (result == -1)
		return;

	if (!retrace_fetch_memory(ep->fd, params->sv, sv, sizeof(sv)))
		return;

	snprintf(info, sizeof(info), "socketpair(%d, %d, %d, %d<->%d)",
	    params->domain, params->type, params->protocol,
	    sv[0], sv[1]);
	set_fdinfo(fdinfos, ep->pid, sv[0], info);
	set_fdinfo(fdinfos, ep->pid, sv[1], info);
}

static void
fclose_postcall(struct retrace_endpoint *ep,
	struct retrace_call_context *context)
{
	struct retrace_fclose_params *params =
	    (struct retrace_fclose_params *)&context->params;
	struct handler_info *hi = ep->handle->user_data;
	struct fdinfo_h *fdinfos = &hi->fdinfos;

	set_streaminfo(fdinfos, ep->pid, params->stream, NULL);
}

static void
close_postcall(struct retrace_endpoint *ep,
	struct retrace_call_context *context)
{
	struct retrace_close_params *params =
	    (struct retrace_close_params *)&context->params;
	struct handler_info *hi = ep->handle->user_data;
	struct fdinfo_h *fdinfos = &hi->fdinfos;

	set_fdinfo(fdinfos, ep->pid, params->fd, NULL);
}

static void
closedir_postcall(struct retrace_endpoint *ep,
	struct retrace_call_context *context)
{
	struct retrace_closedir_params *params =
	    (struct retrace_closedir_params *)&context->params;
	struct handler_info *hi = ep->handle->user_data;
	struct fdinfo_h *fdinfos = &hi->fdinfos;

	set_dirinfo(fdinfos, ep->pid, params->dirp, NULL);
}

void
init_tracefd_handlers(struct retrace_handle *handle)
{
	struct handler_info *handler_info;

	handler_info = handle->user_data;

	SLIST_INIT(&handler_info->fdinfos);

	retrace_add_postcall_handler(handle, RPC_opendir, opendir_postcall);
	retrace_add_postcall_handler(handle, RPC_fdopendir,
	    fdopendir_postcall);
	retrace_add_postcall_handler(handle, RPC_fopen, fopen_postcall);
	retrace_add_postcall_handler(handle, RPC_creat, creat_postcall);
	retrace_add_postcall_handler(handle, RPC_open, open_postcall);
	retrace_add_postcall_handler(handle, RPC_openat, openat_postcall);
	retrace_add_postcall_handler(handle, RPC_socket, socket_postcall);
	retrace_add_postcall_handler(handle, RPC_socketpair, socketpair_postcall);
	retrace_add_postcall_handler(handle, RPC_close, close_postcall);
	retrace_add_postcall_handler(handle, RPC_fclose, fclose_postcall);
	retrace_add_postcall_handler(handle, RPC_closedir, closedir_postcall);

	retrace_add_process_handler(handle, inherit);

	handler_info->tracefds = 1;
}
