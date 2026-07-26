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
 * ptrace event loop.
 *
 * The child stops on every syscall boundary (entry and exit) because
 * we set PTRACE_O_TRACESYSGOOD at the first stop. The high bit (0x80)
 * of the waitpid status distinguishes syscall-stops from real signals.
 *
 * On a syscall-entry stop we:
 *   1. read registers via PTRACE_GETREGSET (NT_PRSTATUS)
 *   2. translate them into a portable frame
 *   3. hand the frame to retrace_engine_wrapper(name, frame)
 *   4. write any arg modifications back via PTRACE_SETREGSET
 *   5. if the engine asked to skip the real syscall, set rax/x0 to the
 *      forced return value and continue with PTRACE_SYSCALL — the
 *      kernel will jump straight to syscall-exit.
 *
 * This file is Linux-only; it compiles to an empty stub elsewhere so
 * the rest of the backend still links on non-Linux dev hosts.
 */

#define _GNU_SOURCE
#include "trace_loop.h"
#include "translate.h"

#include "engine.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <retrace/backend.h>

#ifdef __linux__
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <elf.h>      /* NT_PRSTATUS */
#include <sys/user.h> /* struct user_regs_struct on x86_64 */
#endif

#ifndef WIFSTOPPED
#define WIFSTOPPED(s) 0
#endif
#ifndef WSTOPSIG
#define WSTOPSIG(s) 0
#endif
#ifndef WIFEXITED
#define WIFEXITED(s) 0
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(s) 0
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(s) 0
#endif
#ifndef WTERMSIG
#define WTERMSIG(s) 0
#endif

/* PTRACE_O_TRACESYSGOOD bit 7 in stop signal, used to tell syscall-stops
 * from real-signal stops.
 */
#ifndef PTRACE_O_TRACESYSGOOD
#define PTRACE_O_TRACESYSGOOD 0x00000001
#endif

/* The waitpid stop-signal bit for syscall-stops. */
#define RETRACE_SYSCALL_STOP_BIT 0x80

#ifdef __linux__
static int
set_sysgood_options(pid_t pid)
{
	long rc =
	  ptrace(PTRACE_SETOPTIONS, pid, 0, (void *) (uintptr_t) PTRACE_O_TRACESYSGOOD);
	if (rc < 0) {
		fprintf(
		  stderr, "retrace: ptrace: PTRACE_SETOPTIONS failed: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

static int
read_regset(pid_t pid, struct iovec *iov, void *buf, size_t cap)
{
	iov->iov_base = buf;
	iov->iov_len = cap;
	if (ptrace(PTRACE_GETREGSET, pid, (void *) (uintptr_t) NT_PRSTATUS, iov) < 0) {
		fprintf(stderr, "retrace: ptrace: GETREGSET failed: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

static int
write_regset(pid_t pid, const struct iovec *iov)
{
	if (ptrace(PTRACE_SETREGSET, pid, (void *) (uintptr_t) NT_PRSTATUS, (void *) iov) <
	    0) {
		fprintf(stderr, "retrace: ptrace: SETREGSET failed: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}
#endif /* __linux__ */

int
retrace_ptrace_trace_loop(struct retrace_engine *eng, pid_t child_pid)
{
#ifdef __linux__
	int in_syscall = 0; /* next stop is entry (0) or exit (1) */
	int status;

	if (eng == NULL)
		return -1;

	/* Wait for the initial execve stop (Linux delivers SIGTRAP at that
	 * point for a PTRACE_TRACEME child).
	 */
	while (waitpid(child_pid, &status, 0) < 0) {
		if (errno != EINTR)
			return -1;
	}

	if (!WIFSTOPPED(status)) {
		/* child died before first stop */
		return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	}

	if (set_sysgood_options(child_pid) < 0)
		return -1;

	/* Continue to first syscall entry. */
	if (ptrace(PTRACE_SYSCALL, child_pid, 0, 0) < 0)
		return -1;

	for (;;) {
		while (waitpid(child_pid, &status, 0) < 0) {
			if (errno != EINTR)
				return -1;
		}

		if (WIFEXITED(status))
			return WEXITSTATUS(status);
		if (WIFSIGNALED(status))
			return -1;
		if (!WIFSTOPPED(status))
			return -1;

		int stopsig = WSTOPSIG(status);
		int is_syscall_stop = (stopsig & RETRACE_SYSCALL_STOP_BIT) != 0;

		if (is_syscall_stop && !in_syscall) {
			/* Syscall ENTRY. */
			struct retrace_ptrace_frame frame;
			unsigned char regset_buf[512]; /* ample for either arch */
			struct iovec  iov;

			memset(&frame, 0, sizeof(frame));
			frame.arch = retrace_ptrace_detect_arch();

			if (read_regset(child_pid, &iov, regset_buf, sizeof(regset_buf)) ==
			      0 &&
			    retrace_ptrace_read_regs(&frame, regset_buf, iov.iov_len) == 0 &&
			    frame.syscall_name != NULL) {
				/* Dispatch to the engine. The engine may:
				 *   - leave args alone (allow)
				 *   - set arg_modified[]+arg_out[] (rewrite)
				 *   - set skip_real + forced_retval (skip)
				 */
				retrace_engine_wrapper((char *) frame.syscall_name, &frame);

				if (frame.skip_real) {
					/* Force the syscall to return
					 * forced_retval without running.
					 * On x86_64 we rewrite orig_rax to
					 * -1 so the kernel skips dispatch;
					 * on aarch64 the loop's next stop
					 * will be syscall-exit. Either way
					 * we set the retval register.
					 */
					retrace_ptrace_set_retval(
					  regset_buf, iov.iov_len, frame.forced_retval);
#ifdef __x86_64__
					if (frame.arch == RETRACE_PTRACE_ARCH_X86_64) {
						struct user_regs_struct *r =
						  (struct user_regs_struct *) regset_buf;
						r->orig_rax = (unsigned long long) -1;
						r->rax =
						  (unsigned long long) frame.forced_retval;
					}
#endif
					write_regset(child_pid, &iov);
				} else {
					int    have_write = 0;
					size_t i;

					for (i = 0; i < RETRACE_PTRACE_MAX_ARGS; i++) {
						if (frame.arg_modified[i]) {
							have_write = 1;
							break;
						}
					}
					if (have_write) {
						retrace_ptrace_write_regs(
						  regset_buf, iov.iov_len, &frame);
						write_regset(child_pid, &iov);
					}
				}
			}

			in_syscall = 1;
			if (ptrace(PTRACE_SYSCALL, child_pid, 0, 0) < 0)
				return -1;
			continue;
		}

		if (is_syscall_stop && in_syscall) {
			/* Syscall EXIT. Just continue. */
			in_syscall = 0;
			if (ptrace(PTRACE_SYSCALL, child_pid, 0, 0) < 0)
				return -1;
			continue;
		}

		/* Real signal delivery. Forward it and keep going. */
		if (ptrace(PTRACE_SYSCALL, child_pid, 0, (void *) (uintptr_t) stopsig) < 0)
			return -1;
	}
#else  /* !__linux__ */
	(void) eng;
	(void) child_pid;
	fprintf(stderr, "retrace: ptrace backend is only supported on Linux\n");
	return -1;
#endif /* __linux__ */
}
