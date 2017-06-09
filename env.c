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
#include "env.h"

int RETRACE_IMPLEMENTATION(unsetenv)(const char *name)
{
	real_unsetenv = RETRACE_GET_REAL(unsetenv);
	trace_printf(1, "unsetenv(\"%s\");\n", name);
	return real_unsetenv(name);
}

RETRACE_REPLACE(unsetenv)

int RETRACE_IMPLEMENTATION(putenv)(char *string)
{
	real_putenv = RETRACE_GET_REAL(putenv);
	trace_printf(1, "putenv(\"%s\");\n", string);
	return real_putenv(string);
}

RETRACE_REPLACE(putenv)

char *RETRACE_IMPLEMENTATION(getenv)(const char *envname)
{
	real_getenv = RETRACE_GET_REAL(getenv);
	char *env = real_getenv(envname);
	trace_printf(1, "getenv(\"%s\"); [%s]\n", envname, env);
	return real_getenv(envname);
}

RETRACE_REPLACE(getenv)

int RETRACE_IMPLEMENTATION(uname)(struct utsname *buf)
{
	real_uname = RETRACE_GET_REAL(uname);
	
	int ret = real_uname(buf);
	if (ret == 0)
		trace_printf(1, "uname(); [%s, %s, %s, %s, %s]\n", buf->sysname, buf->nodename,
					buf->release, buf->version, buf->machine);
	else
		trace_printf(1, "uname(); NULL");

	return ret;
}

RETRACE_REPLACE(uname)
