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
#include "pledge.h"

#ifdef __OpenBSD__

int RETRACE_IMPLEMENTATION(pledge)(const char *promises, const char *paths[])
{
	rtr_pledge_t real_pledge;
	int r;
	const char **s;

	real_pledge = RETRACE_GET_REAL(pledge);

	r = real_pledge(promises, paths);

	trace_printf(1, "pledge(\"%s\", ", promises);
	
	if (paths == NULL) {
		trace_printf(0, "NULL");
	} else {
		trace_printf(0, "{");

		s = paths;

		if (*s) {
			trace_printf(0, "\"%s\"", *s);
			s++;
		}

		while (*s) {
			trace_printf(0, ", \"%s\"", *s);
			s++;
		}

		trace_printf(0, "}");
	}

	trace_printf(0, "); [return %d]\n", r);

	return r;
}

RETRACE_REPLACE(pledge)

#endif /* __OpenBSD */
