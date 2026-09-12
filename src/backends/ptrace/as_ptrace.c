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

/*
 * Syscall-lane as-ops (ADR-0016). On the ptrace lane the kernel IS
 * the real implementation: continuing the tracee runs the real
 * syscall, so sched_real allows, cancel skips, and call_real --
 * an in-process invocation on the preload lanes -- means "let the
 * kernel run it". The engine's action scripts drive denial
 * (sandbox), fault injection (modify_return_value_int without
 * call_real) and logging exactly as on the preload lanes; only
 * the frame mechanics differ.
 *
 * Pointer params are MATERIALIZED from the tracee's address space
 * (process_vm_readv) so actions read real strings, never tracee
 * pointers dereferenced in the tracer. v1 materializes string
 * refs ("sz"); other refs surface as NULL (the log's deref-skip
 * path) -- see the ADR's out-of-scope list.
 */

#define _GNU_SOURCE
#include "translate.h"

#include "arch_spec.h"
#include "real_impls.h"
#include "logger.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/uio.h>

/* Cap for materialized strings: PATH_MAX-ish with headroom. */
#define PTRACE_SZ_CAP 4096

/* Chunk size for remote string reads: bounds one read to well
 * under a page so a string ending at a page boundary costs at
 * most one extra chunk, never a full-cap failure.
 */
#define PTRACE_SZ_CHUNK 256

/*
 * ia_call_real stores this ops' return value into t_ctx->ret_val;
 * the engine tail then passes it back through set_ret_val. The
 * sentinel marks "result unknown at the entry stop" so the tail's
 * unconditional write does not turn into a bogus exit override.
 * 2^52+1: exact as a double (a modify_return_value_int value
 * comes through JSON as one) and unreachable by any sane script.
 */
#define PTRACE_RET_DEFERRED ((intptr_t) 4503599627370497LL)

static ssize_t
tracee_read(pid_t pid, void *dst, size_t len, uintptr_t remote_addr)
{
	struct iovec local;
	struct iovec remote;

	local.iov_base = dst;
	local.iov_len = len;
	remote.iov_base = (void *) remote_addr;
	remote.iov_len = len;
	return process_vm_readv(pid, &local, 1, &remote, 1, 0);
}

/* Read a NUL-terminated string from the tracee. Returns a
 * tracer-owned NUL-terminated buffer (caller frees), or 0 when
 * the read fails (actions treat the param as NULL).
 */
static uintptr_t
tracee_read_sz(pid_t pid, uintptr_t remote_addr)
{
	char *buf;
	size_t have = 0;

	buf = (char *) retrace_real_impls.malloc(PTRACE_SZ_CAP + 1);
	if (buf == NULL)
		return 0;

	while (have < PTRACE_SZ_CAP) {
		size_t want = PTRACE_SZ_CAP - have;
		size_t chunk = want < PTRACE_SZ_CHUNK ? want : PTRACE_SZ_CHUNK;
		ssize_t n = tracee_read(pid, buf + have, chunk,
					remote_addr + have);
		size_t i;

		if (n <= 0)
			break;
		for (i = 0; i < (size_t) n; i++) {
			if (buf[have + i] == '\0')
				return (uintptr_t) buf;
		}
		have += (size_t) n;
		if ((size_t) n < chunk)
			break;
	}

	if (have == 0) {
		retrace_real_impls.free(buf);
		return 0;
	}
	buf[have] = '\0';
	return (uintptr_t) buf;
}

static void
ptrace_sched_real(void *arch_spec_ctx, void *real_impl)
{
	struct retrace_ptrace_frame *frame = arch_spec_ctx;

	(void) real_impl;
	/* Continuing runs the real syscall -- the kernel IS the real
	 * implementation (ADR-0016 §3).
	 */
	frame->skip_real = 0;
}

static void
ptrace_cancel_sched_real(void *arch_spec_ctx)
{
	struct retrace_ptrace_frame *frame = arch_spec_ctx;

	frame->skip_real = 1;
	frame->forced_retval = -ENOSYS;
}

static void
ptrace_set_ret_val(void *arch_spec_ctx, intptr_t ret_val)
{
	struct retrace_ptrace_frame *frame = arch_spec_ctx;

	if (!frame->skip_real) {
		/* The call was allowed; only an explicit later modify
		 * may override the kernel's result, at the exit stop.
		 */
		if (ret_val != PTRACE_RET_DEFERRED) {
			frame->exit_override = 1;
			frame->exit_retval = (long) ret_val;
		}
		return;
	}

	/* Libc convention (ret -1, errno set) -> kernel convention
	 * (-errno): a static tracee's syscall wrapper only
	 * understands the latter (ADR-0016 §4).
	 */
	if (ret_val == -1) {
		int e = errno;

		if (e <= 0 || e >= 4096)
			e = EPERM;
		frame->forced_retval = -e;
	} else {
		frame->forced_retval = (long) ret_val;
	}
}

static int
ptrace_setup_params(void			    *arch_spec_ctx,
		    const struct FuncPrototype *proto,
		    struct FuncParam		    params[],
		    int			    *params_cnt)
{
	struct retrace_ptrace_frame *frame = arch_spec_ctx;
	int i;

	if (*params_cnt < proto->params_cnt) {
		log_err("too many prototyped params for '%s', no space for %d more",
			proto->name,
			(*params_cnt - proto->params_cnt) * -1);
		return 0;
	}

	/* Variadic protos (printf/scanf families) have no syscall
	 * counterpart; refuse so the engine skips actions and the
	 * kernel runs the call untouched.
	 */
	if (proto->fmt != FAT_NOVARARGS)
		return 0;

	for (i = 0; i != proto->params_cnt; i++) {
		retrace_real_impls.memset(&params[i].param_meta,
					  0,
					  sizeof(struct ParamMeta));
		retrace_real_impls.memcpy(&params[i].param_meta,
					  &proto->params[i],
					  sizeof(struct ParamMeta));

		params[i].data_type =
			retrace_datatype_get(proto->params[i].type_name);
		params[i].val = 0;
		params[i].free_val = 0;

		if (i < RETRACE_PTRACE_MAX_ARGS)
			params[i].val = (intptr_t) frame->arg_in[i];

		if ((proto->params[i].modifiers & CDM_POINTER) &&
		    params[i].val != 0 &&
		    retrace_real_impls.strcmp(
			    proto->params[i].ref_type_name, "sz") == 0) {
			uintptr_t local = tracee_read_sz(
				frame->tracee_pid,
				(uintptr_t) params[i].val);

			if (local != 0) {
				params[i].val = (intptr_t) local;
				params[i].free_val = 1;
			} else {
				params[i].val = 0;
			}
		} else if (proto->params[i].modifiers & CDM_POINTER) {
			/* Non-string refs surface as NULL: the log's
			 * deref-skip path makes the absence visible
			 * instead of dereferencing tracee memory in
			 * the tracer (ADR-0016 out-of-scope).
			 */
			params[i].val = 0;
		}
	}

	*params_cnt = proto->params_cnt;
	return 1;
}

static intptr_t
ptrace_call_real(void	   *arch_spec_ctx,
		 const void *real_impl,
		 const struct FuncParam params[],
		 int	   params_cnt)
{
	struct retrace_ptrace_frame *frame = arch_spec_ctx;

	(void) real_impl;
	(void) params;
	(void) params_cnt;

	/* "Call the real implementation" on this lane = run the
	 * syscall, which continuing the tracee does. The return
	 * value is unknown at the entry stop; the sentinel keeps
	 * the engine tail from synthesizing one.
	 */
	frame->skip_real = 0;
	return PTRACE_RET_DEFERRED;
}

const struct retrace_as_ops retrace_as_ops_ptrace = {
	.sched_real = ptrace_sched_real,
	.cancel_sched_real = ptrace_cancel_sched_real,
	.set_ret_val = ptrace_set_ret_val,
	.setup_params = ptrace_setup_params,
	.call_real = ptrace_call_real
};
