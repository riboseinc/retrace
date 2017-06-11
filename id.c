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
#include "id.h"

#include <unistd.h>

int RETRACE_IMPLEMENTATION(setuid)(uid_t uid)
{
	rtr_setuid_t real_setuid = RETRACE_GET_REAL(setuid);
	trace_printf(1, "setuid(%d);\n", uid);
	return real_setuid(uid);
}

RETRACE_REPLACE(setuid)

int RETRACE_IMPLEMENTATION(seteuid)(uid_t uid)
{
	rtr_seteuid_t real_seteuid = RETRACE_GET_REAL(seteuid);
	trace_printf(1, "seteuid(%d);\n", uid);
	return real_seteuid(uid);
}

RETRACE_REPLACE(seteuid)

int RETRACE_IMPLEMENTATION(setgid)(gid_t gid)
{
	rtr_setgid_t real_setgid = RETRACE_GET_REAL(setgid);
	trace_printf(1, "setgid(%d);\n", gid);
	return real_setgid(gid);
}

RETRACE_REPLACE(setgid)

gid_t RETRACE_IMPLEMENTATION(getgid)()
{
	rtr_getgid_t real_getgid = RETRACE_GET_REAL(getgid);
	int gid = real_getgid();
	trace_printf(1, "getgid(); [%d]\n", gid);
	return(gid);
}

RETRACE_REPLACE(getgid)

gid_t RETRACE_IMPLEMENTATION(getegid)()
{
	rtr_getegid_t real_getegid = RETRACE_GET_REAL(getegid);
	int egid = real_getegid();
	trace_printf(1, "getegid(); [%d]\n", egid);
	return(egid);
}

RETRACE_REPLACE(getegid)

uid_t RETRACE_IMPLEMENTATION(getuid)()
{
	int redirect_id;
	if (rtr_get_config_single("getuid", ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END, &redirect_id)) {
		trace_printf(1, "getuid(); [redirection in effect: '%i']\n", redirect_id);

		return redirect_id;
	}

	rtr_getuid_t real_getuid = RETRACE_GET_REAL(getuid);

	int uid = real_getuid();
	trace_printf(1, "getuid(); [%d]\n", uid);
	return(uid);
}

RETRACE_REPLACE(getuid)

uid_t RETRACE_IMPLEMENTATION(geteuid)()
{
	int redirect_id;
	if (rtr_get_config_single("geteuid", ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END, &redirect_id)) {
		trace_printf(1, "geteuid(); [redirection in effect: '%i']\n", redirect_id);

		return redirect_id;
	}

	rtr_geteuid_t real_geteuid = RETRACE_GET_REAL(geteuid);

	int euid = real_geteuid();
	trace_printf(1, "geteuid(); [%d]\n", euid);
	return(euid);
}

RETRACE_REPLACE(geteuid)

pid_t RETRACE_IMPLEMENTATION(getpid)(void)
{
	rtr_getpid_t real_getpid = RETRACE_GET_REAL(getpid);
	int pid = real_getpid();
	trace_printf(1, "getpid(); [%d]\n", pid);
	return(pid);
}

RETRACE_REPLACE(getpid)

pid_t RETRACE_IMPLEMENTATION(getppid)(void)
{
	rtr_getppid_t real_getppid = RETRACE_GET_REAL(getppid);
	int ppid = real_getppid();
	trace_printf(1, "getppid(); [%d]\n", ppid);
	return(ppid);
}

RETRACE_REPLACE(getppid)
