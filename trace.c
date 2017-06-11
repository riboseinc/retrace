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

#include "trace.h"
#include "common.h"

#include <sys/types.h>
#include <sys/ptrace.h>

#ifdef __APPLE__
long int RETRACE_IMPLEMENTATION(ptrace)(int request, ...)
#else
long int RETRACE_IMPLEMENTATION(ptrace)(enum __ptrace_request request, ...)
#endif
{
        real_ptrace = RETRACE_GET_REAL(ptrace);
        char *request_str = "UNKNOW";
        int r;
        pid_t pid;
        caddr_t addr;
        int data;

        va_list arglist;
        va_start(arglist, request);

        pid = va_arg(arglist, int);
        addr = va_arg(arglist, void *);
        data = va_arg(arglist, int);

        va_end(arglist);

        switch (request) {
                case PT_TRACE_ME:
                        request_str = "PT_TRACE_ME";
                        break;
		case PT_READ_I:
			request_str = "PT_READ_I";
			break;
		case PT_READ_D:
                        request_str = "PT_READ_D";
                        break;
		case PT_READ_U:
                        request_str = "PT_READ_U";
                        break;
		case PT_WRITE_I:
                        request_str = "PT_WRITE_I";
                        break;
		case PT_WRITE_D:
                        request_str = "PT_WRITE_D";
                        break;
		case PT_WRITE_U:
                        request_str = "PT_WRITE_U";
                        break;
		case PT_CONTINUE:
                        request_str = "PT_CONTINUE";
                        break;
		case PT_KILL:
                        request_str = "PT_KILL";
                        break;
		case PT_STEP:
                        request_str = "PT_STEP";
                        break;
		case PT_DETACH:
                        request_str = "PT_DETACH";
                        break;
#if __APPLE__
		case PT_SIGEXC:
                        request_str = "PT_SIGEXC";
                        break;
		case PT_THUPDATE:
                        request_str = "PT_THUPDATE";
                        break;
		case PT_ATTACHEXC:
                        request_str = "PT_ATTACHEXC";
                        break;
		case PT_FORCEQUOTA:
                        request_str = "PT_FORCEQUOTA";
                        break;
		case PT_DENY_ATTACH:
                        request_str = "PT_DENY_ATTACH";
                        break;
		case PT_FIRSTMACH:
                        request_str = "PT_FIRSTMACH";
                        break;
#else
                case PT_ATTACH:
                        request_str = "PT_ATTACH";
                        break;
                case PTRACE_GETREGS:
                        request_str = "PTRACE_GETREGS";
                        break;
		case PTRACE_SETREGS:
			request_str = "PTRACE_SETREGS";
			break;
		case PTRACE_GETFPREGS:
                        request_str = "PTRACE_GETFPREGS";
                        break;
		case PTRACE_SETFPREGS:
                        request_str = "PTRACE_SETFPREGS";
                        break;	
		case PTRACE_GETFPXREGS:
                        request_str = "PTRACE_GETFPXREGS";
                        break;
		case PTRACE_SETFPXREGS:
                        request_str = "PTRACE_SETFPXREGS";
                        break;
		case PTRACE_SYSCALL:
                        request_str = "PTRACE_SYSCALL";
                        break;
		case PTRACE_SETOPTIONS:
                        request_str = "PTRACE_SETOPTIONS";
                        break;
		case PTRACE_GETEVENTMSG:
                        request_str = "PTRACE_GETEVENTMSG";
                        break;
		case PTRACE_GETSIGINFO:
                        request_str = "PTRACE_GETSIGINFO";
                        break;
		case PTRACE_SETSIGINFO:
                        request_str = "PTRACE_SETSIGINFO";
                        break;
		case PTRACE_GETREGSET:
                        request_str = "PTRACE_GETREGSET";
                        break;
		case PTRACE_SETREGSET:
                        request_str = "PTRACE_SETREGSET";
                        break;
		case PTRACE_SEIZE:
                        request_str = "PTRACE_SEIZE";
                        break;
		case PTRACE_INTERRUPT:
                        request_str = "PTRACE_INTERRUPT";
                        break;
		case PTRACE_LISTEN:
                        request_str = "PTRACE_LISTEN";
                        break;
		case PTRACE_PEEKSIGINFO:
                        request_str = "PTRACE_LISTEN";
                        break;
#endif
        }

        r = real_ptrace(request, pid, addr, data);

        trace_printf(1, "ptrace(\"%s\"(%d), %u, %p, %p) [return: %d];\n", request_str, request, pid, addr, data, r);

        return r;
}

RETRACE_REPLACE(ptrace)

