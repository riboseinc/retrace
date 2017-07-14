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
#include <sys/queue.h>
#include <assert.h>
#include <pthread.h>

#define BLKSZ 65536
#define ALIGNSZ (32 / 8)
#define MAPSZ (BLKSZ / ALIGNSZ / 8)

struct map {
	SLIST_ENTRY(map) next;
	unsigned long int address;
	char map[MAPSZ];
};

SLIST_HEAD(maplist, map);

static int map_bit(const void *p, int set)
{
	static struct maplist maps = SLIST_HEAD_INITIALIZER(maps);
	static pthread_mutex_t maps_mutex = PTHREAD_MUTEX_INITIALIZER;
	struct map *map, *prev;
	unsigned long int address;
	unsigned int offset, bit;
	int old, mask;

	address = p - NULL;
	assert(address % ALIGNSZ == 0);

	pthread_mutex_lock(&maps_mutex);

	prev = NULL;
	SLIST_FOREACH(map, &maps, next) {
		if (map->address > address)
			break;
		prev = map;
	}

	if (prev != NULL && prev->address + BLKSZ > address)
		map = prev;
	else {
		map = real_malloc(sizeof(struct map));
		memset(map, 0, sizeof(struct map));
		map->address = address - address % BLKSZ;
		if (prev == NULL)
			SLIST_INSERT_HEAD(&maps, map, next);
		else
			SLIST_INSERT_AFTER(prev, map, next);
	}

	bit = (address % BLKSZ) / ALIGNSZ;
	offset = bit >> 3;
	bit = bit & 0x7;
	mask = 1 << bit;
	old = (map->map[offset] & mask) >> bit;

	if (set == 0)
		map->map[offset] &= ~mask;
	else
		map->map[offset] |= mask;

	pthread_mutex_unlock(&maps_mutex);

	return old;
}

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
	event_info.function_group = RTR_FUNC_GRP_MEM;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		/* set errno as ENOMEM */
		errno = ENOMEM;
		redirect = 1;
		event_info.extra_info = "redirected : NULL";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;
	}

	if (!redirect) {
		p = real_malloc(bytes);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	if (p != NULL)
		map_bit(p, 1);

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
	event_info.function_group = RTR_FUNC_GRP_MEM;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	if (mem != NULL && map_bit(mem, 0) != 1) {
		event_info.extra_info = "Bad free (possibly already free)";
		event_info.event_flags |= EVENT_FLAGS_PRINT_BEFORE;
	}

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
	event_info.function_group = RTR_FUNC_GRP_MEM;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		/* set errno as ENOMEM */
		errno = ENOMEM;
		redirect = 1;
		event_info.extra_info = "redirected : NULL";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;
	}

	if (!redirect) {
		p = real_calloc(nmemb, size);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	if (p != NULL)
		map_bit(p, 1);

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

	if (ptr != NULL) {
		if (map_bit(ptr, 0) != 1) {
			event_info.extra_info = "Bad realloc (possibly already free)";
			event_info.event_flags |= EVENT_FLAGS_PRINT_BEFORE;
		}
	}

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "realloc";
	event_info.function_group = RTR_FUNC_GRP_MEM;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	if (size > 0 && rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		/* set errno as ENOMEM */
		errno = ENOMEM;
		redirect = 1;
		event_info.extra_info = "redirected : NULL";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;
	}

	if (!redirect) {
		p = real_realloc(ptr, size);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	if (p != NULL)
		map_bit(p, 1);

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
	event_info.function_group = RTR_FUNC_GRP_MEM;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	/* check overlapped memory copying */
	if (abs(dest - src) < n) {
		overlapped = 1;
		event_info.extra_info = "The memory areas must not overlap. It may arise bugs. Please refer the man page.";
		event_info.event_flags = EVENT_FLAGS_PRINT_BACKTRACE;
	}

	p = real_memcpy(dest, src, n);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

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
	event_info.function_group = RTR_FUNC_GRP_MEM;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

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
	event_info.function_group = RTR_FUNC_GRP_MEM;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_END;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

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
	event_info.function_group = RTR_FUNC_GRP_MEM;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	p = real_memccpy(dest, src, c, n);

	retrace_log_and_redirect_after(&event_info);

	return p;
}

RETRACE_REPLACE(memccpy, void *, (void *dest, const void *src, int c, size_t n), (dest, src, c, n))

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

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "mmap";
	event_info.function_group = RTR_FUNC_GRP_MEM;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	/* mmap fuzzing */
	if ((flags & MAP_ANONYMOUS) && rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		/* set errno as ENOMEM */
		errno = ENOMEM;
		redirect = 1;
		event_info.extra_info = "redirected : (void *) -1";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;
	}

	if (!redirect) {
		p = real_mmap(addr, length, prot, flags, fd, offset);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

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
	event_info.function_group = RTR_FUNC_GRP_MEM;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	ret = real_munmap(addr, length);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(munmap, int, (void *addr, size_t length), (addr, length))

#ifndef __APPLE__

#ifdef __FreeBSD__
int RETRACE_IMPLEMENTATION(brk)(const void *addr)
#else
int RETRACE_IMPLEMENTATION(brk)(void *addr)
#endif
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&addr};

	int ret = -1;

	double fail_chance = 0;
	int redirect = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "brk";
	event_info.function_group = RTR_FUNC_GRP_MEM;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	/* brk fuzzing */
	if (rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		/* set errno as ENOMEM */
		errno = ENOMEM;
		redirect = 1;
		event_info.extra_info = "redirected : -1";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;
	}

	if (!redirect) {
		ret = real_brk((void *) addr);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(brk, int, (void *addr), (addr))

#if __OpenBSD__
void *RETRACE_IMPLEMENTATION(sbrk)(int increment)
#else
void *RETRACE_IMPLEMENTATION(sbrk)(intptr_t increment)
#endif
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&increment};

	void *p = (void *) -1;

	double fail_chance = 0;
	int redirect = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "sbrk";
	event_info.function_group = RTR_FUNC_GRP_MEM;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_POINTER;
	event_info.return_value = &p;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	/* sbrk fuzzing */
	if (rtr_get_config_single("memoryfuzzing", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &fail_chance) &&
		rtr_get_fuzzing_flag(fail_chance)) {
		/* set errno as ENOMEM */
		errno = ENOMEM;
		redirect = 1;
		event_info.extra_info = "redirected : (void *) -1";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED | EVENT_FLAGS_PRINT_BACKTRACE;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;
	}

	if (!redirect) {
		p = real_sbrk(increment);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return p;
}

#if __OpenBSD__
RETRACE_REPLACE(sbrk, void *, (long int increment), (increment))
#else
RETRACE_REPLACE(sbrk, void *, (intptr_t increment), (increment))
#endif

#endif
