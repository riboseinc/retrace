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
#include "netfuzz.h"

#include "rtr-netdb.h"

#include <arpa/inet.h>

struct hostent *RETRACE_IMPLEMENTATION(gethostbyname)(const char *name)
{
	struct hostent *hent;

	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&name};

	int err;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "gethostbyname";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &hent;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_net_fuzzing(NET_FUNC_ID_GETHOSTNAME, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		h_errno = err;
		hent = NULL;
	} else {
		hent = real_gethostbyname(name);
		if (hent)
			event_info.return_value_type = PARAMETER_TYPE_STRUCT_HOSTEN;
		else
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return hent;
}

RETRACE_REPLACE(gethostbyname, struct hostent *, (const char *name), (name))


struct hostent *RETRACE_IMPLEMENTATION(gethostbyaddr)(const void *addr, socklen_t len, int type)
{
	struct hostent *hent;

	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&addr};

	int err;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "gethostbyaddr";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &hent;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_net_fuzzing(NET_FUNC_ID_GETHOSTADDR, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		h_errno = err;
		hent = NULL;
	} else {
		hent = real_gethostbyaddr(addr, len, type);
		if (hent)
			event_info.return_value_type = PARAMETER_TYPE_STRUCT_HOSTEN;
		else
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return hent;
}

RETRACE_REPLACE(gethostbyaddr, struct hostent *,
	(const void *addr, socklen_t len, int type), (addr, len, type))


void RETRACE_IMPLEMENTATION(sethostent)(int stayopen)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&stayopen};

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "sethostent";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	real_sethostent(stayopen);

	retrace_log_and_redirect_after(&event_info);

}

RETRACE_REPLACE(sethostent, void, (int stayopen), (stayopen))


void RETRACE_IMPLEMENTATION(endhostent)(void)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_END};
	void const *parameter_values[] = {};

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "endhostent";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	real_endhostent();

	retrace_log_and_redirect_after(&event_info);

}

RETRACE_REPLACE(endhostent, void, (void), ())


struct hostent *RETRACE_IMPLEMENTATION(gethostent)(void)
{
	struct hostent *hent;

	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_END};
	void const *parameter_values[] = {};

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "gethostent";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &hent;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	hent = real_gethostent();
	if (hent) {
		event_info.return_value_type = PARAMETER_TYPE_STRUCT_HOSTEN;
	} else
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return hent;
}

RETRACE_REPLACE(gethostent, struct hostent *, (void), ())


struct hostent *RETRACE_IMPLEMENTATION(gethostbyname2)(const char *name, int af)
{
	struct hostent *hent;

	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&name, &af};

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "gethostbyname2";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &hent;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	hent = real_gethostbyname2(name, af);
	if (hent) {
		event_info.return_value_type = PARAMETER_TYPE_STRUCT_HOSTEN;
	} else
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return hent;
}

RETRACE_REPLACE(gethostbyname2, struct hostent *, (const char *name, int af),
	(name, af))


int RETRACE_IMPLEMENTATION(getaddrinfo)(const char *node, const char *service,
	const struct addrinfo *hints, struct addrinfo **res)
{
	int ret;

	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRING, PARAMETER_TYPE_STRUCT_ADDRINFO, PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&node, &service, &hints, res};

	int err;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "getaddrinfo";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_net_fuzzing(NET_FUNC_ID_GETADDRINFO, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		if (err == HOST_NOT_FOUND)
			ret = EAI_NODATA;
		else if (err == TRY_AGAIN)
			ret = EAI_AGAIN;
		else
			ret = EAI_MEMORY;
	} else {
		ret = real_getaddrinfo(node, service, hints, res);
		if (ret == 0) {
			event_info.return_value_type = PARAMETER_TYPE_STRUCT_ADDRINFO;
			event_info.return_value = res;
		} else
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(getaddrinfo, int,
	(const char *node, const char *service, const struct addrinfo *hints,
	    struct addrinfo **res),
	(node, service, hints, res))


void RETRACE_IMPLEMENTATION(freeaddrinfo)(struct addrinfo *res)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&res};

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "freeaddrinfo";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	real_freeaddrinfo(res);

	retrace_log_and_redirect_after(&event_info);
}

RETRACE_REPLACE(freeaddrinfo, void, (struct addrinfo *res), (res))
