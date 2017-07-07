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
#include "rtr-netdb.h"

#include <arpa/inet.h>

struct hostent *RETRACE_IMPLEMENTATION(gethostbyname)(const char *name)
{
	struct hostent *hent;

	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_STRING, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&name};

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "gethostbyname";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &hent;

	retrace_log_and_redirect_before(&event_info);

	hent = real_gethostbyname(name);
	if (hent) {
		event_info.return_value_type = PARAMETER_TYPE_STRUCT_HOSTEN;
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

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "gethostbyaddr";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &hent;

	retrace_log_and_redirect_before(&event_info);

	hent = real_gethostbyaddr(addr, len, type);
	if (hent) {
		event_info.return_value_type = PARAMETER_TYPE_STRUCT_HOSTEN;
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
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;

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
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;

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
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &hent;

	retrace_log_and_redirect_before(&event_info);

	hent = real_gethostent();
	if (hent) {
		event_info.return_value_type = PARAMETER_TYPE_STRUCT_HOSTEN;
	}

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
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &hent;

	retrace_log_and_redirect_before(&event_info);

	hent = real_gethostbyname2(name, af);
	if (hent) {
		event_info.return_value_type = PARAMETER_TYPE_STRUCT_HOSTEN;
	}

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

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "getaddrinfo";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;

	retrace_log_and_redirect_before(&event_info);

	ret = real_getaddrinfo(node, service, hints, res);
	if (ret == 0) {
		event_info.return_value_type = PARAMETER_TYPE_STRUCT_ADDRINFO;
		event_info.return_value = res;
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
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;

	retrace_log_and_redirect_before(&event_info);

	real_freeaddrinfo(res);

	retrace_log_and_redirect_after(&event_info);
}

RETRACE_REPLACE(freeaddrinfo, void, (struct addrinfo *res), (res))
