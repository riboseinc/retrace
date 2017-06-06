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

int
setuid(uid_t uid)
{
	real_setuid = dlsym(RTLD_NEXT, "setuid");
	trace_printf(1, "setuid(%d);\n", uid);
	return real_setuid(uid);
}

int
seteuid(uid_t uid)
{
	real_seteuid = dlsym(RTLD_NEXT, "seteuid");
	trace_printf(1, "seteuid(%d);\n", uid);
	return real_seteuid(uid);
}

int
setgid(gid_t gid)
{
	real_setgid = dlsym(RTLD_NEXT, "setgid");
	trace_printf(1, "setgid(%d);\n", gid);
	return real_setgid(gid);
}

gid_t
getgid()
{
	real_getgid = dlsym(RTLD_NEXT, "getgid");
	trace_printf(1, "getgid();\n");
	return real_getgid();
}

gid_t
getegid()
{
	real_getegid = dlsym(RTLD_NEXT, "getegid");
	trace_printf(1, "getegid();\n");
	return real_getegid();
}

uid_t
getuid()
{
	int redirect_id;
	if (get_redirect("getuid", ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END, &redirect_id)) {
		trace_printf(1, "getuid(); [redirection in effect: '%i']\n", redirect_id);

		return redirect_id;
	}

	real_getuid = dlsym(RTLD_NEXT, "getuid");
	trace_printf(1, "getuid();\n");
	return real_getuid();
}

uid_t
geteuid()
{
	int redirect_id;
	if (get_redirect("geteuid", ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END, &redirect_id)) {
		trace_printf(1, "geteuid(); [redirection in effect: '%i']\n", redirect_id);

		return redirect_id;
	}

	real_geteuid = dlsym(RTLD_NEXT, "geteuid");
	trace_printf(1, "geteuid();\n");
	return real_geteuid();
}

pid_t
getpid(void)
{
	real_getpid = dlsym(RTLD_NEXT, "getpid");
	trace_printf(1, "getpid();\n");
	return real_getpid();
}

pid_t
getppid(void)
{
	real_getppid = dlsym(RTLD_NEXT, "getppid");
	trace_printf(1, "getppid(); [%d]\n", real_getppid());
	return real_getppid();
}
