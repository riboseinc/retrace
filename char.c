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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"
#include "file.h"

#include "char.h"

int
putc(int c, FILE *stream)
{
    real_putc = dlsym(RTLD_NEXT, "putc");
    real_fileno = dlsym(RTLD_NEXT, "fileno");
    int fd = real_fileno(stream);

    trace_printf(1, "putc(");
    if (c == '\n')
        trace_printf(0, "%s\\n%s", VAR, RST);
    else if (c == '\t')
        trace_printf(0, "%s\\t%s", VAR, RST);
    else if (c == '\r')
        trace_printf(0, "%s\\r%s", VAR, RST);
    else if (c == '\0')
        trace_printf(0, "%s(nullterm)%s", VAR, RST);
    else
        trace_printf(0, "%c", c);

    trace_printf(0, ", %d);\n", fd);

    return real_putc(c, stream);
}

int
toupper(int c)
{
    real_toupper = dlsym(RTLD_NEXT, "toupper");
    trace_printf(1, "toupper(\"%s\");\n", &c);
    return real_toupper(c);
}

int
tolower(int c)
{
    real_tolower = dlsym(RTLD_NEXT, "tolower");
    trace_printf(1, "tolower(\"%c\");\n", &c);
    return real_tolower(c);
}
