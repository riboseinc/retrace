/*
 * Copyright (c) 2017-2022 [Ribose Inc](https://www.ribose.com).
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
	event_info.function_group = RTR_FUNC_GRP_SYS;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_STRING;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_ctime_r(timep, buf);
	if (!r)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

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
	event_info.function_group = RTR_FUNC_GRP_SYS;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	r = real_ctime(timep);
	if (!r)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(ctime, char *, (const time_t *timep), (timep))

#ifdef HAVE_GETTIMEOFDAY_WITHOUT_TIMEZONE
int RETRACE_IMPLEMENTATION(gettimeofday)(struct timeval *restrict tv, void *restrict tzp)
#else
int RETRACE_IMPLEMENTATION(gettimeofday)(struct timeval *restrict tv, struct timezone *restrict tz)
#endif
{
	struct rtr_event_info event_info;

#if HAVE_GETTIMEOFDAY_WITHOUT_TIMEZONE
	unsigned int parameter_types[] = {PARAMETER_TYPE_TIMEVAL, PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void const *parameter_values[] = {(void const*)&tv, (void const*)&tzp};
#else
	unsigned int parameter_types[] = {PARAMETER_TYPE_TIMEVAL, PARAMETER_TYPE_TIMEZONE, PARAMETER_TYPE_END};
	void const *parameter_values[] = {(void const*)&tv, (void const*)&tz};
#endif

	int ret;

#if HAVE_GETTIMEOFDAY_WITHOUT_TIMEZONE
	struct timezone *tz;
	tz = (struct timezone *)tzp;
#endif

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "gettimeofday";
	event_info.function_group = RTR_FUNC_GRP_SYS;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_gettimeofday(tv, tz);
	if (ret < 0)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

#if HAVE_GETTIMEOFDAY_WITHOUT_TIMEZONE
RETRACE_REPLACE(gettimeofday, int, (struct timeval *restrict tv, void *restrict tzp), (tv, tzp))
#else
RETRACE_REPLACE(gettimeofday, int, (struct timeval *restrict tv, struct timezone *restrict tz), (tv, tz))
#endif
