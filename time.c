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
#include "rtr-time.h"

char *RETRACE_IMPLEMENTATION(ctime_r)(const time_t *timep, char *buf)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&timep, &buf};
	char *r = NULL;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "ctime_r";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_ctime_r(timep, buf);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(ctime_r, char *, (const time_t *timep, char *buf), (timep, buf))

char *RETRACE_IMPLEMENTATION(ctime)(const time_t *timep)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&timep};
	char *r;


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "ctime";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_ctime(timep);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(ctime, char *, (const time_t *timep), (timep))

#if defined(__APPLE__) || defined(__NetBSD__)
int RETRACE_IMPLEMENTATION(gettimeofday)(struct timeval *tv, void *tzp)
#else
int RETRACE_IMPLEMENTATION(gettimeofday)(struct timeval *tv, struct timezone *tz)
#endif
{
	struct rtr_event_info event_info;

#if defined(__APPLE__) || defined(__NetBSD__)
	unsigned int parameter_types[] = {PARAMETER_TYPE_TIMEVAL, PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&tv, &tzp};
#else
	unsigned int parameter_types[] = {PARAMETER_TYPE_TIMEVAL, PARAMETER_TYPE_TIMEZONE, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&tv, &tz};
#endif
	int ret;
#if defined(__APPLE__) || defined(__NetBSD__)
	struct timezone *tz;

	tz = (struct timezone *)tzp;
#endif


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "gettimeofday";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	retrace_log_and_redirect_before(&event_info);

	ret = real_gettimeofday(tv, tz);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

#if defined(__APPLE__) || defined(__NetBSD__)
RETRACE_REPLACE(gettimeofday, int, (struct timeval *tv, void *tzp), (tv, tsp))
#else
RETRACE_REPLACE(gettimeofday, int, (struct timeval *tv, struct timezone *tz), (tv, tz))
#endif
