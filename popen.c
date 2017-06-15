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
#include "popen.h"

FILE *RETRACE_IMPLEMENTATION(popen)(const char *command, const char *type)
{
	FILE *ret;
	rtr_popen_t real_popen;
	rtr_fileno_t real_fileno;

	real_popen	= RETRACE_GET_REAL(popen);
	real_fileno	= RETRACE_GET_REAL(fileno);

	ret = real_popen(command, type);

	trace_printf(1, "popen(\"%s\", \"%s\"); [%d]\n", command, type, real_fileno(ret));

	return ret;
}

RETRACE_REPLACE(popen)

int RETRACE_IMPLEMENTATION(pclose)(FILE *stream)
{
	int ret;
	rtr_pclose_t real_pclose;
	rtr_fileno_t real_fileno;

	real_pclose = RETRACE_GET_REAL(pclose);
	real_fileno = RETRACE_GET_REAL(fileno);

	ret = real_pclose(stream);

	trace_printf(1, "pclose(%d); [%d]\n", real_fileno(stream), ret);

	return ret;
}

RETRACE_REPLACE(pclose)
