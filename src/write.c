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
#include "strinject.h"
#include "malloc.h"
#include "write.h"
#include "httpredirect.h"

ssize_t RETRACE_IMPLEMENTATION(write)(int fd, const void *buf, size_t nbytes)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR,
					  PARAMETER_TYPE_MEMORY_BUFFER,
					  PARAMETER_TYPE_INT,
					  PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fd, &nbytes, &buf, &nbytes, NULL};
	size_t  real_nbytes = nbytes;
	ssize_t ret = 0;
	int incompleteio = 0;
	size_t incompleteio_limit = 0;

	int redirected = 0;

	void *inject_buffer;
	size_t inject_len;

	int enable_inject = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "write";
	event_info.function_group = RTR_FUNC_GRP_FILE | RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	if (rtr_get_config_single("incompleteio", ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END, &incompleteio_limit)) {
		incompleteio = 1;
		real_nbytes = rtr_get_fuzzing_random() % nbytes;
		if (real_nbytes <= incompleteio_limit) {
			real_nbytes = incompleteio_limit;
		}
		if (real_nbytes > nbytes) {
			real_nbytes = nbytes;
		}

		redirected = 1;
	} else if (rtr_str_inject(STRINJECT_FUNC_WRITE, (void *)buf, nbytes, &inject_buffer, &inject_len)) {
		redirected = 1;
		enable_inject = 1;

		parameter_values[1] = &inject_len;
		parameter_values[2] = &inject_buffer;
		parameter_values[3] = &inject_len;
	}

	if (redirected) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;
	}

	retrace_log_and_redirect_before(&event_info);

	rtr_http_sniff_request(fd, buf, real_nbytes);

	ret = real_write(fd,
			enable_inject ? inject_buffer : buf,
			enable_inject ? inject_len : real_nbytes);
	if (ret < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	if (enable_inject && inject_buffer != buf)
		real_free(inject_buffer);

	return ret;
}

RETRACE_REPLACE(write, ssize_t, (int fd, const void *buf, size_t nbytes),
	(fd, buf, nbytes))
