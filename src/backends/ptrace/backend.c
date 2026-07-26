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
 * ptrace backend: statically-linked Linux binaries.
 *
 * LD_PRELOAD cannot reach a static binary — its PLT is empty, so there
 * is nothing to interpose. ptrace gives us the syscall boundary
 * directly: PTRACE_TRACEME + PTRACE_SYSCALL stops at every syscall
 * entry/exit. That is the only general mechanism for tracing static
 * targets.
 *
 * Per the v2 plan, preload-elf owns dynamic binaries (probe() returns
 * 1 for any Linux ELF); this backend owns static binaries (probe()
 * returns 1 only when the target is ET_EXEC *and* has no PT_INTERP).
 * The two backends are mutually exclusive on a given target, so the
 * registry's rank-based selection never has to break a tie between
 * them.
 *
 * translate_frame is intentionally NULL: the ptrace backend does not
 * receive a native frame from a trampoline — it synthesises its own
 * frame in the trace loop. See trace_loop.c.
 */

#define _GNU_SOURCE
#include <retrace/backend.h>
#include "trace_loop.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef __linux__
#include <sys/ptrace.h>
#include <elf.h>
#endif

/* ELF header + program header peek for static-binary detection. The
 * target is "static" iff:
 *   - e_type == ET_EXEC (not a PIE; PIEs are ET_DYN and need an interp)
 *   - no PT_INTERP program-header entry (no dynamic linker)
 */
#ifdef __linux__
static int
elf_target_is_static(const char *target_path)
{
	int	      fd;
	unsigned char eident[EI_NIDENT];
	Elf64_Ehdr    ehdr;
	Elf64_Phdr    phdr;
	size_t	      i;
	ssize_t	      n;

	if (target_path == NULL)
		return 0;

	fd = open(target_path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return 0;

	n = read(fd, eident, sizeof(eident));
	if (n != (ssize_t) sizeof(eident))
		goto no;
	if (memcmp(eident, ELFMAG, SELFMAG) != 0)
		goto no;

	/* Rewind to read the full Elf64_Ehdr from the start. */
	if (lseek(fd, 0, SEEK_SET) != 0)
		goto no;
	n = read(fd, &ehdr, sizeof(ehdr));
	if (n != (ssize_t) sizeof(ehdr))
		goto no;

	/* ET_EXEC means non-PIE. ET_DYN would need the dynamic linker.
	 * Either way, the canonical test is "no PT_INTERP".
	 */
	if (ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN)
		goto no;

	/* Walk program headers looking for PT_INTERP. */
	if (lseek(fd, (off_t) ehdr.e_phoff, SEEK_SET) < 0)
		goto no;
	for (i = 0; i < ehdr.e_phnum; i++) {
		n = read(fd, &phdr, sizeof(phdr));
		if (n != (ssize_t) sizeof(phdr))
			goto no;
		if (phdr.p_type == PT_INTERP) {
			/* Found an interpreter => dynamically linked. */
			close(fd);
			return 0;
		}
	}

	close(fd);
	/* No PT_INTERP and ET_EXEC => statically linked. ET_DYN with no
	 * interp is a static-PIE, also traceable via ptrace.
	 */
	return 1;
no:
	close(fd);
	return 0;
}
#endif /* __linux__ */

static int
ptrace_probe(struct retrace_engine *eng, const char *target_path)
{
	(void) eng;
#ifdef __linux__
	if (target_path == NULL)
		return 0;
	return elf_target_is_static(target_path);
#else
	(void) target_path;
	return 0;
#endif
}

static pid_t
ptrace_spawn(struct retrace_engine *eng,
	     const char *target_path,
	     char *const argv[],
	     char *const envp[])
{
	(void) eng;

	if (target_path == NULL)
		return RETRACE_BACKEND_NOT_FOUND;

#ifdef __linux__
	{
		pid_t pid = fork();

		if (pid < 0)
			return RETRACE_BACKEND_INTERNAL;

		if (pid == 0) {
			/* Child: ask to be traced, then exec. The kernel
			 * will deliver a SIGTRAP to the parent at the
			 * exec boundary.
			 */
			if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0)
				_exit(126);

			if (envp != NULL)
				execve(target_path, argv, envp);
			else
				execv(target_path, argv);
			_exit(127);
		}

		/* Parent: enter the trace loop. It returns the child's
		 * exit status once the tracee terminates.
		 */
		retrace_ptrace_trace_loop(eng, pid);
		return pid;
	}
#else
	(void) argv;
	(void) envp;
	return RETRACE_BACKEND_UNSUPPORTED;
#endif
}

static pid_t
ptrace_attach(struct retrace_engine *eng, pid_t target_pid)
{
	(void) eng;
	if (target_pid <= 0)
		return RETRACE_BACKEND_NOT_FOUND;

#ifdef __linux__
	if (ptrace(PTRACE_ATTACH, target_pid, 0, 0) < 0)
		return RETRACE_BACKEND_PERMISSION;

	retrace_ptrace_trace_loop(eng, target_pid);
	return target_pid;
#else
	(void) target_pid;
	return RETRACE_BACKEND_UNSUPPORTED;
#endif
}

static int
ptrace_detach(struct retrace_engine *eng)
{
	(void) eng;
#ifdef __linux__
	/* PTRACE_DETACH requires the PID. In the current design the
	 * trace loop is synchronous and the tracee has either exited
	 * or is mid-stop under our control; detach is therefore a
	 * hook for the engine's shutdown path. The actual detach (if
	 * any) is performed when the loop observes the child exiting.
	 * Hook left here so the backend advertises detach support to
	 * retrace_backend_select consumers.
	 */
	return RETRACE_BACKEND_OK;
#else
	return RETRACE_BACKEND_UNSUPPORTED;
#endif
}

static const retrace_backend_t ptrace_backend = {
	.name = "ptrace",
	.description = "ptrace syscall interposition for statically-linked Linux binaries",
	.rank = RETRACE_BACKEND_RANK_PREFERRED,
	.probe = ptrace_probe,
	.spawn = ptrace_spawn,
	.attach = ptrace_attach,
	.detach = ptrace_detach,
	.translate_frame = NULL,
};

__attribute__((constructor)) static void
register_ptrace_backend(void)
{
	retrace_backend_register(&ptrace_backend);
}
