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
#include "str.h"
#include "malloc.h"

#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

void *RETRACE_IMPLEMENTATION(malloc)(size_t bytes)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&bytes};

	void *p = NULL;

	double fail_chance = 0;
	int redirect = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "malloc";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;

	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		/* set errno as ENOMEM */
		errno = ENOMEM;
		redirect = 1;
		event_info.extra_info = "redirected : NULL";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
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

	memset(&event_info, 0, sizeof(event_info));
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

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "calloc";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;

	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		/* set errno as ENOMEM */
		errno = ENOMEM;
		redirect = 1;
		event_info.extra_info = "redirected : NULL";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
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

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "realloc";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;

	retrace_log_and_redirect_before(&event_info);

	if (size > 0 && rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		/* set errno as ENOMEM */
		errno = ENOMEM;
		redirect = 1;
		event_info.extra_info = "redirected : NULL";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
	}

	if (!redirect)
		p = real_realloc(ptr, size);

	retrace_log_and_redirect_after(&event_info);

	return p;
}

RETRACE_REPLACE(realloc, void *, (void *ptr, size_t size), (ptr, size))

void *RETRACE_IMPLEMENTATION(memcpy)(void *dest, const void *src, size_t n)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_POINTER, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&dest, &src, &n};

	void *p = NULL;

	int overlapped = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "memcpy";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;

	retrace_log_and_redirect_before(&event_info);

	/* check overlapped memory copying */
	if (abs(dest - src) < n) {
		overlapped = 1;
		event_info.extra_info = "The memory areas must not overlap. It may arise bugs. Please refer the man page.";
		event_info.event_flags = EVENT_FLAGS_PRINT_BACKTRACE;
	}

	p = real_memcpy(dest, src, n);

	retrace_log_and_redirect_after(&event_info);

	return p;
}

RETRACE_REPLACE(memcpy, void *, (void *dest, const void *src, size_t n), (dest, src, n))

void *RETRACE_IMPLEMENTATION(memmove)(void *dest, const void *src, size_t n)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_POINTER, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&dest, &src, &n};

	void *p = NULL;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "memmove";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;

	retrace_log_and_redirect_before(&event_info);

	p = real_memmove(dest, src, n);

	retrace_log_and_redirect_after(&event_info);

	return p;
}

RETRACE_REPLACE(memmove, void *, (void *dest, const void *src, size_t n), (dest, src, n))

void RETRACE_IMPLEMENTATION(bcopy)(const void *src, void *dest, size_t n)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_POINTER, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&src, &dest, &n};

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "bcopy";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;

	retrace_log_and_redirect_before(&event_info);

	real_bcopy(src, dest, n);

	retrace_log_and_redirect_after(&event_info);
}

RETRACE_REPLACE(bcopy, void, (const void *src, void *dest, size_t n), (src, dest, n))

void *RETRACE_IMPLEMENTATION(memccpy)(void *dest, const void *src, int c, size_t n)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_POINTER,
		PARAMETER_TYPE_CHAR, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&dest, &src, &c, &n};

	void *p = NULL;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "memccpy";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;

	retrace_log_and_redirect_before(&event_info);

	p = real_memccpy(dest, src, c, n);

	retrace_log_and_redirect_after(&event_info);

	return p;
}

RETRACE_REPLACE(memccpy, void *, (void *dest, const void *src, int c, size_t n), (dest, src, c, n))

#define RTR_MMAP_MAX_PROT_STRLEN		128
#define RTR_MMAP_MAX_FLAGS_STRLEN		128

static const struct ts_info mmap_prot_ts[] = {
	{PROT_READ, "PROT_READ"},
	{PROT_WRITE, "PROT_WRITE"},
	{PROT_EXEC, "PROT_EXEC"},
#ifndef __APPLE__
	{PROT_GROWSDOWN, "PROT_GROWSDOWN"},
	{PROT_GROWSUP, "PROT_GROWSUP"},
#endif
	{-1, NULL}
};

static const struct ts_info mmap_flags_ts[] = {
	{MAP_SHARED, "MAP_SHARED"},
	{MAP_PRIVATE, "MAP_PRIVATE"},
	{MAP_FIXED, "MAP_FIXED"},
	{MAP_ANONYMOUS, "MAP_ANONYMOUS"},
	{-1, NULL}
};

void *RETRACE_IMPLEMENTATION(mmap)(void *addr, size_t length, int prot, int flags,
	int fd, off_t offset)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_INT, PARAMETER_TYPE_INT, PARAMETER_TYPE_INT,
		PARAMETER_TYPE_INT, PARAMETER_TYPE_LONG, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&addr, &length, &prot, &flags, &fd, &offset};

	void *p = (void *) -1;

	double fail_chance = 0;
	int redirect = 0;

	char prot_str[RTR_MMAP_MAX_PROT_STRLEN + 1];
	char flags_str[RTR_MMAP_MAX_FLAGS_STRLEN + 1];

	/* get description for protection of mapping */
	rtr_get_type_string(prot, mmap_prot_ts, prot_str, sizeof(prot_str));
	if (real_strlen(prot_str) == 0)
		real_strcpy(prot_str, "PROT_NONE");

	rtr_get_type_string(flags, mmap_flags_ts, flags_str, sizeof(flags_str));

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "mmap";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;

	retrace_log_and_redirect_before(&event_info);

	/* mmap fuzzing */
	if ((flags & MAP_ANONYMOUS) && rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		/* set errno as ENOMEM */
		errno = ENOMEM;
		redirect = 1;
		event_info.extra_info = "redirected : (void *) -1";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
	}

	if (!redirect)
		p = real_mmap(addr, length, prot, flags, fd, offset);

	retrace_log_and_redirect_after(&event_info);

	return p;
}

RETRACE_REPLACE(mmap, void *, (void *addr, size_t length, int prot, int flags,
	int fd, off_t offset), (addr, length, prot, flags, fd, offset))

int RETRACE_IMPLEMENTATION(munmap)(void *addr, size_t length)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&addr, &length};
	int ret;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "munmap";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;

	retrace_log_and_redirect_before(&event_info);

	ret = real_munmap(addr, length);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(munmap, int, (void *addr, size_t length), (addr, length))

#ifndef __APPLE__

int RETRACE_IMPLEMENTATION(brk)(void *addr)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&addr};

	int ret = -1;

	double fail_chance = 0;
	int redirect = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "brk";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;

	retrace_log_and_redirect_before(&event_info);

	/* brk fuzzing */
	if (rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		/* set errno as ENOMEM */
		errno = ENOMEM;
		redirect = 1;
		event_info.extra_info = "redirected : -1";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
	}

	if (!redirect)
		ret = real_brk(addr);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(brk, int, (void *addr), (addr))

void *RETRACE_IMPLEMENTATION(sbrk)(intptr_t increment)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&increment};

	void *p = (void *) -1;

	double fail_chance = 0;
	int redirect = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "sbrk";
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;

	retrace_log_and_redirect_before(&event_info);

	/* sbrk fuzzing */
	if (rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		/* set errno as ENOMEM */
		errno = ENOMEM;
		redirect = 1;
		event_info.extra_info = "redirected : (void *) -1";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
	}

	if (!redirect)
		p = real_sbrk(increment);

	retrace_log_and_redirect_after(&event_info);

	return p;
}

RETRACE_REPLACE(sbrk, void *, (intptr_t increment), (increment))

#endif
