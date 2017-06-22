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
#include "file.h"
#include "char.h"
#include "str.h"

#ifdef __FreeBSD__
#define _DONT_USE_CTYPE_INLINE_
#include <runetype.h>
#undef putc
#endif 
#include <ctype.h>

#include <string.h>

static void
trace_putc(const char *name, int c, FILE* stream)
{
	static char specials[] = "\nn\rr\tt";
	char *p;

	p = c == '\0' ? " 0" : real_strchr(specials, c);

	trace_printf(0,
	    p == NULL ? "%s('%c', %d)\n" : "%s(" VAR "'\\%c'" RST ", %d)\n",
	    name, p == NULL ? c : *(p + 1), real_fileno(stream));
}

int
RETRACE_IMPLEMENTATION(putc)(int c, FILE *stream)
{
	trace_putc("putc", c, stream);

	return (real_putc(c, stream));
}

RETRACE_REPLACE(putc, int, (int c, FILE *stream), (c, stream))


#ifndef __APPLE__
int
RETRACE_IMPLEMENTATION(_IO_putc)(int c, FILE *stream)
{
	trace_putc("__IO_putc", c, stream);

	return (real__IO_putc(c, stream));
}

RETRACE_REPLACE(_IO_putc, int, (int c, FILE *stream), (c, stream))

#endif

int
RETRACE_IMPLEMENTATION(toupper)(int c)
{
	trace_printf(1, "toupper('%c');\n", c);
	return (real_toupper(c));
}

RETRACE_REPLACE(toupper, int, (int c), (c))


int
RETRACE_IMPLEMENTATION(tolower)(int c)
{
	trace_printf(1, "tolower('%c');\n", c);
	return (real_tolower(c));
}

RETRACE_REPLACE(tolower, int, (int c), (c))
