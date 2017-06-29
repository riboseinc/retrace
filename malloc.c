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
#include "malloc.h"

#include <stdlib.h>

static int init_rand = 0;

void *RETRACE_IMPLEMENTATION(malloc)(size_t bytes)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&bytes};
	void *p = NULL;
	double fail_chance = 0;
	int redirect = 0;


	event_info.function_name = "malloc";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;
	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance)) {
		long int random_value;

		if (!init_rand) {
			srand (time(NULL));
			init_rand = 1;
		}

		random_value = rand();

		if (random_value <= (RAND_MAX * fail_chance)) {
			redirect = 1;
		}
	}

	if (!redirect)
		p = real_malloc(bytes);

	retrace_log_and_redirect_after(&event_info);

        return p;
}

RETRACE_REPLACE(malloc, void *, (size_t bytes), (bytes))

void RETRACE_IMPLEMENTATION(free)(void *mem)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&mem};


	event_info.function_name = "free";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;

	retrace_log_and_redirect_before(&event_info);

	real_free(mem);

	retrace_log_and_redirect_after(&event_info);

}

RETRACE_REPLACE(free, void, (void *mem), (mem))

void *RETRACE_IMPLEMENTATION(calloc)(size_t nmemb, size_t size)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&nmemb, &size};
	void *p = NULL;
	double fail_chance = 0;
	int redirect = 0;


	event_info.function_name = "calloc";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;

	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance)) {
		long int random_value;

		if (!init_rand) {
			srand (time(NULL));
			init_rand = 1;
		}

		random_value = rand();

		if (random_value <= (RAND_MAX * fail_chance)) {
			redirect = 1;
		}
	}

	if (!redirect)
		p = real_calloc(nmemb, size);

	retrace_log_and_redirect_after(&event_info);

        return p;
}

RETRACE_REPLACE(calloc, void *, (size_t nmemb, size_t size), (nmemb, size))

void *RETRACE_IMPLEMENTATION(realloc)(void *ptr, size_t size)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&ptr, &size};
	void *p = NULL;
	double fail_chance;
	int redirect = 0;


	event_info.function_name = "realloc";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;

	retrace_log_and_redirect_before(&event_info);

	if (size > 0 && rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance)) {
		long int random_value;

		if (!init_rand) {
			srand (time(NULL));
			init_rand = 1;
		}

		random_value = rand();

		if (random_value <= (RAND_MAX * fail_chance)) {
			redirect = 1;
		}
	}

	if (!redirect)
		p = real_realloc(ptr, size);

	retrace_log_and_redirect_after(&event_info);

        return p;
}

RETRACE_REPLACE(realloc, void *, (void *ptr, size_t size), (ptr, size))
