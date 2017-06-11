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

static void print_fds(int max_fd, fd_set *fds)
{
        int fd;
        for (fd = 0; fd < max_fd; fd++)
        {
                if (FD_ISSET(fd, fds))
                        trace_printf(1, "%d,", fd);
        }

        return;
}

int RETRACE_IMPLEMENTATION(select)(int nfds, fd_set *readfds, fd_set *writefds,
                        fd_set *exceptfds, struct timeval *timeout)
{
        rtr_select_t real_select = RETRACE_GET_REAL(select);
        int ret;

        // print original fdsets
        if (readfds)
        {
                trace_printf(1, "select(): input readfds: ");
                print_fds(nfds, readfds);
                trace_printf(1, "\n");
        }

        if (writefds)
        {
                trace_printf(1, "select(): input writefds: ");
                print_fds(nfds, writefds);
                trace_printf(1, "\n");
        }

        if (exceptfds)
        {
                trace_printf(1, "select(): input exceptfds: ");
                print_fds(nfds, writefds);
                trace_printf(1, "\n");
        }

        trace_printf(1, "select(): timeout:[%ld, %ld]\n", timeout->tv_sec, timeout->tv_usec);

        // call select function
        ret = real_select(nfds, readfds, writefds, exceptfds, timeout);
        if (ret == 0)
        {
                trace_printf(1, "select() timeout expired.\n");
        }
        else if (ret > 0)
        {
                if (readfds)
                {
                        trace_printf(1, "select(): output readfds: ");
                        print_fds(nfds, readfds);
                        trace_printf(1, "\n");
                }

                if (writefds)
                {
                        trace_printf(1, "select(): output writefds: ");
                        print_fds(nfds, writefds);
                        trace_printf(1, "\n");
                }

                if (exceptfds)
                {
                        trace_printf(1, "select(): output exceptfds: ");
                        print_fds(nfds, writefds);
                        trace_printf(1, "\n");
                }
        }
        else
        {
                trace_printf(1, "select(): failed.");
        }

        return ret;
}

RETRACE_REPLACE(select)
