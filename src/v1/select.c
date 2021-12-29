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
#include "select.h"

static void
copy_fd_set(fd_set *dest, const fd_set *src)
{
	if (src)
		*dest = *src;
	else
		FD_ZERO(dest);
}

int
RETRACE_IMPLEMENTATION(select)(int nfds, fd_set *readfds, fd_set *writefds,
			fd_set *exceptfds, struct timeval *timeout)
{
	fd_set inr, inw, inx;
	int ret;

	char str_timeout[128];
	const char *read_str = "read";
	const char *write_str = "write";
	const char *except_str = "except";
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FD_SET, PARAMETER_TYPE_FD_SET, PARAMETER_TYPE_FD_SET, PARAMETER_TYPE_END};
	void const *parameter_values[] = {
			&read_str, &nfds, &inr, &readfds,
			&write_str, &nfds, &inw, &writefds,
			&except_str, &nfds, &inx, &exceptfds
	};

	memset(&event_info, 0, sizeof(event_info));

	event_info.function_name = "select";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	copy_fd_set(&inr, readfds);
	copy_fd_set(&inw, writefds);
	copy_fd_set(&inx, exceptfds);

	ret = real_select(nfds, readfds, writefds, exceptfds, timeout);

	if (timeout != NULL) {
		sprintf(str_timeout, "timeout: %ld %ld", timeout->tv_sec, (long) timeout->tv_usec);
		event_info.extra_info = str_timeout;
	} else {
		event_info.extra_info = "no timeout";
	}

	if (ret < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return (ret);
}

RETRACE_REPLACE(select, int,
	(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	    struct timeval *timeout),
	(nfds, readfds, writefds, exceptfds, timeout))
