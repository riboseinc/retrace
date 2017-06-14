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

void RETRACE_IMPLEMENTATION(free)(void *mem)
{
	rtr_free_t real_free;

	real_free = RETRACE_GET_REAL(free);

	trace_printf(1, "free(%p);\n", mem);

	real_free(mem);
}

RETRACE_REPLACE(free)

void *RETRACE_IMPLEMENTATION(malloc)(size_t bytes)
{
	void *p;
	rtr_malloc_t real_malloc;

	real_malloc = RETRACE_GET_REAL(malloc);

	p = real_malloc(bytes);

	trace_printf(1, "malloc(%d); [%p]\n", bytes, p);

	return p;
}

RETRACE_REPLACE(malloc)

void *RETRACE_IMPLEMENTATION(realloc)(void *ptr, size_t size)
{
        void *p;

        rtr_realloc_t real_realloc = RETRACE_GET_REAL(realloc);
        p = real_realloc(ptr, size);
        trace_printf(1, "realloc(%p, %d); [%p]\n", ptr, size, p);

        return p;
}

RETRACE_REPLACE(realloc)
