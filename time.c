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
#include "rtr-time.h"

char *RETRACE_IMPLEMENTATION(ctime_r)(const time_t *timep, char *buf)
{
	char *r;
	r = real_ctime_r(timep, buf);

	trace_printf(1, "ctime_r(\"%u\", \"%s\");\n", timep ? timep : 0, buf);

	return r;
}

RETRACE_REPLACE(ctime_r, char *, (const time_t *timep, char *buf), (timep, buf))

char *RETRACE_IMPLEMENTATION(ctime)(const time_t *timep)
{
	char *r;

	r = real_ctime(timep);

	trace_printf(1, "ctime(\"%u\") [return: %s];\n", timep ? timep : 0, r);

	return r;
}

RETRACE_REPLACE(ctime, char *, (const time_t *timep), (timep))

#if defined(__APPLE__) || defined(__NetBSD__)
int RETRACE_IMPLEMENTATION(gettimeofday)(struct timeval *tv, void *tzp)
#else
int RETRACE_IMPLEMENTATION(gettimeofday)(struct timeval *tv, struct timezone *tz)
#endif
{
	int ret;
#if defined(__APPLE__) || defined(__NetBSD__)
	struct timezone *tz;

	tz = (struct timezone *)tzp;
#endif

	ret = real_gettimeofday(tv, tz);
	if (ret == 0) {
		int tz_minuteswest = 0;
		int tz_dsttime = 0;
		time_t tv_sec;
		suseconds_t tv_usec;

		tv_sec	= tv->tv_sec;
		tv_usec	= tv->tv_usec;

		if (tz != NULL) {
			tz_minuteswest	= tz->tz_minuteswest;
			tz_dsttime	= tz->tz_dsttime;
		}

		trace_printf(1, "gettimeofday(timeval:[%ld, %ld], timezone:[%d, %d]);\n",
				tv_sec, tv_usec, tz_minuteswest, tz_dsttime);
	} else
		trace_printf(1, "gettimeofday(); -1\n");

	return ret;
}

#if defined(__APPLE__) || defined(__NetBSD__)
RETRACE_REPLACE(gettimeofday, int, (struct timeval *tv, void *tzp), (tv, tsp))
#else
RETRACE_REPLACE(gettimeofday, int, (struct timeval *tv, struct timezone *tz), (tv, tz))
#endif
