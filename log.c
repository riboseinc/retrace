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
#include "log.h"
#include "printf.h"

#include <syslog.h>
#include <stdarg.h>

#define OPTION_MAX_BUFFER 128

static const char *
facility_to_string(int facility)
{
	const char *str;

	switch (facility) {
	case LOG_AUTH:
		str = "LOG_AUTH";
		break;
	case LOG_AUTHPRIV:
		str = "LOG_AUTHPRIV";
		break;
	case LOG_CRON:
		str = "LOG_CRON";
		break;
	case LOG_DAEMON:
		str = "LOG_DAEMON";
		break;
	case LOG_FTP:
		str = "LOG_FTP";
		break;
	case LOG_KERN:
		str = "LOG_KERN";
		break;
	case LOG_LOCAL0:
		str = "LOG_LOCAL0";
		break;
	case LOG_LOCAL1:
		str = "LOG_LOCAL1";
		break;
	case LOG_LOCAL2:
		str = "LOG_LOCAL2";
		break;
	case LOG_LOCAL3:
		str = "LOG_LOCAL3";
		break;
	case LOG_LOCAL4:
		str = "LOG_LOCAL4";
		break;
	case LOG_LOCAL5:
		str = "LOG_LOCAL5";
		break;
	case LOG_LOCAL6:
		str = "LOG_LOCAL6";
		break;
	case LOG_LOCAL7:
		str = "LOG_LOCAL7";
		break;
	case LOG_LPR:
		str = "LOG_LPR";
		break;
	case LOG_MAIL:
		str = "LOG_MAIL";
		break;
	case LOG_NEWS:
		str = "LOG_NEWS";
		break;
	case LOG_SYSLOG:
		str = "LOG_SYSLOG";
		break;
	case LOG_USER:
		str = "LOG_USER";
		break;
	case LOG_UUCP:
		str = "LOG_UUCP";
		break;
	default:
		str = "UNKNOWN";
		break;
	}

	return str;
}

const char *
priority_to_string(int level)
{
	char *str;

	switch (level) {
	case LOG_EMERG:
		str = "LOG_EMERG";
		break;
	case LOG_ALERT:
		str = "LOG_ALERT";
		break;
	case LOG_CRIT:
		str = "LOG_CRIT";
		break;
	case LOG_ERR:
		str = "LOG_ERR";
		break;
	case LOG_WARNING:
		str = "LOG_WARNING";
		break;
	case LOG_NOTICE:
		str = "LOG_NOTICE";
		break;
	case LOG_INFO:
		str = "LOG_INFO";
		break;
	case LOG_DEBUG:
		str = "LOG_DEBUG";
		break;
	default:
		str = "UNKNOWN";
		break;
	}

	return str;
}

static void
levelmask_to_string(int mask, char *buf)
{
	int space_used = 0;

	if (mask & LOG_MASK(LOG_EMERG))
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_EMERG|");

	if (mask & LOG_MASK(LOG_ALERT))
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_ALERT|");

	if (mask & LOG_MASK(LOG_CRIT))
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_CRI|");

	if (mask & LOG_MASK(LOG_ERR))
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_ERR|");

	if (mask & LOG_MASK(LOG_WARNING))
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_WARNING|");

	if (mask & LOG_MASK(LOG_NOTICE))
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_NOTICE|");

	if (mask & LOG_MASK(LOG_INFO))
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_INFO|");

	if (mask & LOG_MASK(LOG_DEBUG))
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_DEBUG|");
}

#define OPTION_MAX_BUFFER 128

static void
option_to_string(int option, char *buf)
{
	int space_used = 0;

	if (option & LOG_CONS)
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_CONST|");

	if (option & LOG_NDELAY)
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_NDELAY|");

	if (option & LOG_ODELAY)
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_ODELAY|");

	if (option & LOG_NOWAIT)
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_NOWAIT|");

	if (option & LOG_PERROR)
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_PERROR|");

	if (option & LOG_PID)
		space_used += real_snprintf(buf + space_used, OPTION_MAX_BUFFER - space_used, "LOG_PID|");
}

void RETRACE_IMPLEMENTATION(openlog)(const char *ident, int option, int facility)
{
	char option_str[OPTION_MAX_BUFFER + 1];
	const char *facility_str = NULL;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING,
					  PARAMETER_TYPE_INT | PARAMETER_FLAG_STRING_NEXT,
					  PARAMETER_TYPE_INT | PARAMETER_FLAG_STRING_NEXT,
					  PARAMETER_TYPE_END};
	void *parameter_values[] = {&ident, &option, &option_str, &facility, &facility_str};

	facility_str = facility_to_string(facility);
	option_to_string(option, option_str);

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "openlog";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;
	retrace_log_and_redirect_before(&event_info);

	real_openlog(ident, option, facility);

	retrace_log_and_redirect_after(&event_info);
}

RETRACE_REPLACE(openlog, void, (const char *ident, int option, int facility), (ident, option, facility))


void RETRACE_IMPLEMENTATION(syslog)(int priority, const char *format, ...)
{
	const char *priority_str = NULL;
	va_list ap;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT | PARAMETER_FLAG_STRING_NEXT,
					  PARAMETER_TYPE_PRINTF_FORMAT,
					  PARAMETER_TYPE_END};
	void *parameter_values[] = {&priority, &priority_str, &format, &ap};

	priority_str = priority_to_string(priority);

	va_start(ap, format);
	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "syslog";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;
	retrace_log_and_redirect_before(&event_info);
	va_end(ap);

	va_start(ap, format);
	real_vsyslog(priority, format, ap);
	va_end(ap);

	va_start(ap, format);
	retrace_log_and_redirect_after(&event_info);
	va_end(ap);
}

RETRACE_REPLACE_VOID_V(syslog, void, (int priority, const char *format, ...), format, real_vsyslog, (priority, format, ap))

void RETRACE_IMPLEMENTATION(closelog)(void)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_END};

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "closelog";
	event_info.parameter_types = parameter_types;
	event_info.return_value_type = PARAMETER_TYPE_END;
	retrace_log_and_redirect_before(&event_info);

	real_closelog();

	retrace_log_and_redirect_after(&event_info);
}

RETRACE_REPLACE(closelog, void, (void), ())

void RETRACE_IMPLEMENTATION(vsyslog)(int priority, const char *format, va_list ap)
{
	const char *priority_str = NULL;
	va_list ap_copy_after;
	va_list ap_copy_before;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT | PARAMETER_FLAG_STRING_NEXT,
					  PARAMETER_TYPE_PRINTF_FORMAT,
					  PARAMETER_TYPE_END};
	void *parameter_values[] = {&priority, &priority_str, &format, &ap_copy_before};

	priority_str = priority_to_string(priority);

	va_copy(ap_copy_after, ap);
	va_copy(ap_copy_before, ap);

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "vsyslog";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;
	retrace_log_and_redirect_before(&event_info);

	real_vsyslog(priority, format, ap);

	parameter_values[3] = &ap_copy_after;
	retrace_log_and_redirect_after(&event_info);

	va_end(ap_copy_after);
	va_end(ap_copy_before);
}

RETRACE_REPLACE(vsyslog, void, (int priority, const char *format, va_list ap), (priority, format, ap))

int RETRACE_IMPLEMENTATION(setlogmask)(int mask)
{
	char mask_str[OPTION_MAX_BUFFER + 1];
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT | PARAMETER_FLAG_STRING_NEXT,
					  PARAMETER_TYPE_END};
	void *parameter_values[] = {&mask, &mask_str};
	int r;

	levelmask_to_string(mask, mask_str);

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "setlogmask";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &r;
	retrace_log_and_redirect_before(&event_info);

	r = real_setlogmask(mask);

	retrace_log_and_redirect_after(&event_info);

	return r;
}

RETRACE_REPLACE(setlogmask, int, (int mask), (mask))
