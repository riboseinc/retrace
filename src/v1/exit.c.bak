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
#include "exit.h"

void RETRACE_IMPLEMENTATION(exit)(int status)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void *parameter_values[] = {&status};

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "exit";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;
	event_info.event_flags = EVENT_FLAGS_PRINT_BEFORE;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	real_exit(status);
}

RETRACE_REPLACE(exit, void, (int status), (status))

#ifdef __linux__
int RETRACE_IMPLEMENTATION(on_exit)(void (*function)(int, void *), void *arg)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void *parameter_values[] = {&function, &arg};
	int r = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "on_exit";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_on_exit(function, arg);
	if (r != 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(on_exit, int, (void (*function)(int, void *), void *arg), (function, arg))

int RETRACE_IMPLEMENTATION(__cxa_atexit)(void (*function)(void), void *p1, void *p2)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void *parameter_values[] = {&function};
	int r;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "atexit";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real___cxa_atexit(function, p1, p2);
	if (r != 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(__cxa_atexit, int, (void (*function)(void), void *p1, void *p2), (function, p1, p2))
#endif

#ifndef __OpenBSD__
int RETRACE_IMPLEMENTATION(atexit)(void (*function)(void))
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void *parameter_values[] = {&function};
	int r;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "atexit";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_atexit(function);
	if (r != 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(atexit, int, (void (*function)(void)), (function))
#endif


void RETRACE_IMPLEMENTATION(_exit)(int status)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void *parameter_values[] = {&status};

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "_exit";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;
	event_info.event_flags = EVENT_FLAGS_PRINT_BEFORE;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	real__exit(status);
}

RETRACE_REPLACE(_exit, void, (int status), (status))
