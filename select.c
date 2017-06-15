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

#include "common.h"
#include "select.h"

static void
copy_fd_set(fd_set *dest, const fd_set *src)
{
	if (src)
		*dest = *src;
	else
		FD_ZERO(dest);
}

static void
print_fds(const char *set, int nfds, fd_set *in, fd_set *out)
{
	int fd, comma = 0;

	if (out == NULL)
		return;

	trace_printf(0, "(%s:", set);
	for (fd = 0; fd < nfds; fd++) {
		if (FD_ISSET(fd, in)) {
			trace_printf(0, "%.*s%.*s%d", comma, ",",
			    FD_ISSET(fd, out) ? 1 : 0, "+", fd);

			if (comma == 0)
				comma = 1;
		}
	}
	trace_printf(0, ")");
}

int
RETRACE_IMPLEMENTATION(select)(int nfds, fd_set *readfds, fd_set *writefds,
			fd_set *exceptfds, struct timeval *timeout)
{
	rtr_select_t real_select;
	fd_set inr, inw, inx;
	int ret;

	copy_fd_set(&inr, readfds);
	copy_fd_set(&inw, writefds);
	copy_fd_set(&inx, exceptfds);

	real_select = RETRACE_GET_REAL(select);
	ret = real_select(nfds, readfds, writefds, exceptfds, timeout);

	if (timeout != NULL)
		trace_printf(1, "select (timeout: %lds %ldus) [=%d]",
		    timeout->tv_sec, timeout->tv_usec, ret);
	else
		trace_printf(1, "select (no timeout)[=%d]", ret);

	print_fds("read", nfds, &inr, readfds);
	print_fds("write", nfds, &inr, writefds);
	print_fds("except", nfds, &inr, exceptfds);
	trace_printf(0, "\n");

	return (ret);
}

RETRACE_REPLACE(select)
