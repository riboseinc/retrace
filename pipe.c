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
#include "pipe.h"

#include <unistd.h>

int RETRACE_IMPLEMENTATION(pipe)(int pipefd[2])
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR | PARAMETER_FLAG_OUTPUT_VARIABLE,
					  PARAMETER_TYPE_FILE_DESCRIPTOR | PARAMETER_FLAG_OUTPUT_VARIABLE,
					  PARAMETER_TYPE_END};
	void const *parameter_values[] = {&pipefd[0], &pipefd[1]};
	int ret = 0;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "pipe";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_pipe(pipefd);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(pipe, int, (int pipefd[2]), (pipefd))


#ifndef __APPLE__

int RETRACE_IMPLEMENTATION(pipe2)(int pipefd[2], int flags)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR | PARAMETER_FLAG_OUTPUT_VARIABLE,
					  PARAMETER_TYPE_FILE_DESCRIPTOR | PARAMETER_FLAG_OUTPUT_VARIABLE,
					  PARAMETER_TYPE_INT,
					  PARAMETER_TYPE_END};
	void const *parameter_values[] = {&pipefd[0], &pipefd[1], &flags};
	int ret;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "pipe2";
	event_info.function_group = RTR_FUNC_GRP_FILE;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_pipe2(pipefd, flags);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(pipe2, int, (int pipefd[2], int flags), (pipefd, flags))

#endif
