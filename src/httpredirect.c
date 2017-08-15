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

#include "config.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>

#include "common.h"
#include "sock.h"
#include "scanf.h"
#include "printf.h"
#include "malloc.h"
#include "file.h"
#include "read.h"
#include "str.h"
#include "select.h"

struct rtr_http_redirect_info *
rtr_setup_http_redirect(const struct sockaddr *addr)
{
	const char *match_ip, *dir;
	int port, match_port;
	RTR_CONFIG_HANDLE config = RTR_CONFIG_START;
	struct rtr_http_redirect_info *pinfo = NULL;
	char ip[128];

	if (addr->sa_family == AF_INET) {
		port = ntohs(((struct sockaddr_in *)addr)->sin_port);
		inet_ntop(AF_INET, &(((struct sockaddr_in *)addr)->sin_addr),
		    ip, sizeof(ip));
	} else {
		port = ntohs(((struct sockaddr_in6 *)addr)->sin6_port);
		inet_ntop(AF_INET6,
		    &(((struct sockaddr_in6 *)addr)->sin6_addr), ip,
		    sizeof(ip));
	}

	for (;;) {
		if (rtr_get_config_multiple(&config, "httpredirect",
		    ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT,
		    ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_END,
		    &match_ip, &match_port, &dir) == 0)
			break;

		if (match_port == port && real_strcmp(match_ip, ip) == 0) {
			pinfo = real_malloc(
			    sizeof(struct rtr_http_redirect_info));
			if (pinfo) {
				memset(pinfo, 0,
				    sizeof(struct rtr_http_redirect_info));
				pinfo->filefd = -1;
				pinfo->dir = dir;
				pinfo->sniffing = 1;
			}
			break;
		}
	}

	return pinfo;
}

static struct rtr_http_redirect_info *lookup_redirect_info(int fd)
{
	struct descriptor_info *pinfo;

	pinfo = file_descriptor_get(fd);

	return (pinfo ? pinfo->http_redirect : NULL);
}

static ssize_t select_and_read(int fd, void *buf, size_t len)
{
	fd_set fds;
	ssize_t readlen;

	for (;;) {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		if (real_select(fd + 1, &fds, NULL, NULL, NULL) == -1) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		readlen = real_recv(fd, buf, len, MSG_DONTWAIT);
		if (readlen == 0)
			readlen = -1;
		break;
	}
	return readlen;
}

static void discard_response(int fd)
{
	const size_t READ_MAX = 1024;
	size_t readlen, contentlen = 0;
	char buf[READ_MAX + 1], *p, *end;
	unsigned int bufoff = 0;
	int chunked = 0, inheaders = 1;

	/*
	 * parse headers taking transfer encoding and/or content length
	 */
	while (inheaders) {
		readlen = select_and_read(fd, buf + bufoff,
		    READ_MAX - bufoff);

		if (readlen == -1)
			return;

		bufoff += readlen;
		buf[bufoff] = '\0';
		for (p = buf; inheaders; p = end + 2) {
			end = real_strstr(p, "\r\n");
			if (end == p)
				inheaders = 0;

			if (end == NULL)
				/* not a full line */
				break;

			*end = '\0';
			if (strcasestr(p, "Content-Length: ") == p)
				contentlen = atol(p + 16);

			if (strcasestr(p, "Transfer-Encoding: ") == p)
				if (strcasestr(p, "chunked"))
					chunked = 1;
		}

		bufoff -= p - buf;
		real_memmove(buf, p, bufoff);
		buf[bufoff] = '\0';
	}

	if (chunked) {
		while (real_strstr(buf, "\r\n0\r\n") == NULL) {

			if (bufoff > 4) {
				real_memmove(buf, buf + bufoff - 4, 4);
				bufoff = 4;
				buf[bufoff] = '\0';
			}

			readlen = select_and_read(fd, buf + bufoff,
			    READ_MAX - bufoff);

			if (readlen == -1)
				return;

			bufoff += readlen;
			buf[bufoff] = '\0';
		}
	} else {
		contentlen -= bufoff;
		while (contentlen) {

			readlen = contentlen;
			if (readlen > READ_MAX)
				readlen = READ_MAX;

			readlen = select_and_read(fd, buf, readlen);

			if (readlen == -1)
				break;

			contentlen -= readlen;
		}
	}
}

const char *locate_url(char *buf)
{
	char *url, *end;

	if (real_strstr(buf, "GET /") != buf)
		return 0;

	url = buf + 5;
	end = real_strchr(url, ' ');

	if (end == NULL || end == url ||
	    (real_strcmp(end, " HTTP/1.0") && real_strcmp(end, " HTTP/1.1")))
		return 0;

	*end = '\0';

	return url;
}

static int open_redirect_file(const char *dir, const char *url)
{
	char *path;
	int retval;

	path = real_malloc(strlen(dir) + real_strlen(url) + 2);
	if (path == NULL)
		return -1;

	real_sprintf(path, "%s/%s", dir, url);

	retval = real_open(path, O_RDONLY);

	real_free(path);

	return retval;
}

static size_t size_redirect_file(int fd)
{
	off_t retval;

	retval = lseek(fd, 0, SEEK_END);
	if (retval == -1)
		return 0;

	lseek(fd, 0, SEEK_SET);

	return retval;
}

/*
 * rtr_http_sniff_request sniffs the first line of a request for
 * "GET %s HTTP/1.n". If it matches it then opens any corresponding redirect
 * file in preparation for rtr_http_redirect_response.
 */
void
rtr_http_sniff_request(int fd, const void *buf, size_t len)
{
	struct rtr_http_redirect_info *info;
	const char *url;
	size_t snifflen;
	char *p;

	info = lookup_redirect_info(fd);

	if (info == NULL || info->sniffing == 0)
		return;

	snifflen = len;
	if (snifflen > RTR_HTTP_SNIFFLEN - info->sniffoff)
		snifflen = RTR_HTTP_SNIFFLEN - info->sniffoff;

	real_memcpy(info->sniffbuf + info->sniffoff, buf, snifflen);

	info->sniffoff += snifflen;
	info->sniffbuf[info->sniffoff] = '\0';

	p = real_strstr(info->sniffbuf, "\r\n");
	if (p != NULL) {
		*p = '\0';
		url = locate_url(info->sniffbuf);
		if (url)
			info->filefd = open_redirect_file(info->dir, url);
		if (info->filefd != -1) {
			info->remainder = size_redirect_file(info->filefd);
			if (info->remainder == 0) {
				real_close(info->filefd);
				info->filefd = -1;
			}
		}
		info->sniffing = 0;
	}

	if (info->sniffoff == RTR_HTTP_SNIFFLEN)
		info->sniffing = 0;
}

/*
 * rtr_http_redirect_response checks to see if we're redirecting (ie.
 * rtr_http_sniff_request set up a file descriptor for redirected response.) If
 * so it copies the content of the file into the buffer. After file contents
 * are exhauseted, it reads and discards the server's own response before
 * returning.
 */
size_t
rtr_http_redirect_response(int fd, void *buf, size_t len, int flags)
{
	struct rtr_http_redirect_info *info;
	ssize_t readlen;

	info = lookup_redirect_info(fd);

	if (info == NULL || info->filefd == -1 || info->remainder == 0)
		return 0;

	info->sniffing = 1;

	readlen = info->remainder;
	if (readlen > len)
		readlen = len;

	readlen = real_read(info->filefd, buf, readlen);
	if (readlen <= 0)
		/* TODO: error handling */
		return 0;

	if (flags & MSG_PEEK)
		lseek(info->filefd, -readlen, SEEK_CUR);
	else
		info->remainder -= readlen;

	if (info->remainder == 0) {
		real_close(info->filefd);
		info->sniffoff = 0;
		info->sniffing = 1;
		info->filefd = -1;

		discard_response(fd);
	}

	errno = 0;
	return readlen;
}
