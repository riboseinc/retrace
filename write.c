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
#include "write.h"

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


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "write";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;

	if (rtr_get_config_single("incompleteio", ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END, &incompleteio_limit)) {
		incompleteio = 1;
		real_nbytes = rtr_get_fuzzing_random() % nbytes;
		if (real_nbytes <= incompleteio_limit) {
			real_nbytes = incompleteio_limit;
		}
		if (real_nbytes > nbytes) {
			real_nbytes = nbytes;
		}
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
	}

	retrace_log_and_redirect_before(&event_info);

	ret = real_write(fd, buf, real_nbytes);

	if (incompleteio)
		event_info.print_backtrace = 1;

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(write, ssize_t, (int fd, const void *buf, size_t nbytes),
	(fd, buf, nbytes))
