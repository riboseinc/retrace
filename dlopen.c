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
#include "dlopen.h"

void *RETRACE_IMPLEMENTATION(dlopen)(const char *filename, int flag)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&filename, &flag};
	void *r = NULL;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "dlopen";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_dlopen(filename, flag);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(dlopen, void *, (const char *filename, int flag),
	(filename, flag))


char *RETRACE_IMPLEMENTATION(dlerror)(void)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_END};
	char *r = NULL;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "dlerror";
	event_info.parameter_types = parameter_types;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_dlerror();

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(dlerror, char *, (void), ())


#if !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
#ifdef HAVE_ATOMIC_BUILTINS
void *RETRACE_IMPLEMENTATION(dlsym)(void *handle, const char *symbol)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&handle, &symbol};
	void *r = NULL;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "dlsym";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_dlsym(handle, symbol);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(dlsym, void *, (void *handle, const char *symbol),
	(handle, symbol))
#endif
#endif

int RETRACE_IMPLEMENTATION(dlclose)(void *handle)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&handle};
	int r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "dlclose";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_dlclose(handle);

	retrace_log_and_redirect_after(&event_info);

	trace_printf(1, "dlclose(%p); [return: %d]\n", handle, r);

	return r;
}

RETRACE_REPLACE(dlclose, int, (void *handle), (handle))
