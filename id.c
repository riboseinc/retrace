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
#include "id.h"

#include <unistd.h>

int
RETRACE_IMPLEMENTATION(setuid)(uid_t uid)
{
	real_setuid = rtr_dlsym(rtr_setuid);
	trace_printf(1, "setuid(%d);\n", uid);
	return real_setuid(uid);
}

RETRACE_REPLACE (setuid)

int
RETRACE_IMPLEMENTATION(seteuid)(uid_t uid)
{
	real_seteuid = rtr_dlsym(rtr_seteuid);
	trace_printf(1, "seteuid(%d);\n", uid);
	return real_seteuid(uid);
}

RETRACE_REPLACE (seteuid)

int
RETRACE_IMPLEMENTATION(setgid)(gid_t gid)
{
	real_setgid = rtr_dlsym(rtr_setgid);
	trace_printf(1, "setgid(%d);\n", gid);
	return real_setgid(gid);
}

RETRACE_REPLACE (setgid)

gid_t
RETRACE_IMPLEMENTATION(getgid)()
{
	real_getgid = rtr_dlsym(rtr_getgid);
	trace_printf(1, "getgid();\n");
	return real_getgid();
}

RETRACE_REPLACE (getgid)

gid_t
RETRACE_IMPLEMENTATION(getegid)()
{
	real_getegid = rtr_dlsym(rtr_getegid);
	trace_printf(1, "getegid();\n");
	return real_getegid();
}

RETRACE_REPLACE (getegid)

uid_t
RETRACE_IMPLEMENTATION(getuid)()
{
	int redirect_id;
	if (get_redirect("getuid", ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END, &redirect_id)) {
		trace_printf(1, "getuid(); [redirection in effect: '%i']\n", redirect_id);

		return redirect_id;
	}

	real_getuid = rtr_dlsym(rtr_getuid);
	trace_printf(1, "getuid();\n");
	return real_getuid();
}

RETRACE_REPLACE (getuid)

uid_t
RETRACE_IMPLEMENTATION(geteuid)()
{
	int redirect_id;
	if (get_redirect ("geteuid", ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END, &redirect_id)) {
		trace_printf(1, "geteuid(); [redirection in effect: '%i']\n", redirect_id);

		return redirect_id;
	}

	real_geteuid = rtr_dlsym(rtr_geteuid);
	trace_printf(1, "geteuid();\n");
	return real_geteuid();
}

RETRACE_REPLACE (geteuid)

pid_t
RETRACE_IMPLEMENTATION(getpid)(void)
{
	real_getpid = rtr_dlsym(rtr_getpid);
	trace_printf(1, "getpid();\n");
	return real_getpid();
}

RETRACE_REPLACE (getpid)

pid_t
RETRACE_IMPLEMENTATION(getppid)(void)
{
	real_getppid = rtr_dlsym(rtr_getppid);
	trace_printf(1, "getppid(); [%d]\n", real_getppid());
	return real_getppid();
}

RETRACE_REPLACE (getppid)

