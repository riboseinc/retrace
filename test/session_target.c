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
 * Session/tree target (TODO.supervisor/04): builds a small
 * process forest under one supervisor, WITHOUT env(1) -- a
 * macOS CI dyld lesson: system binaries are arm64e-signed and
 * dyld TERMINATES a protected process that requests insertion of
 * an incompatible (arm64) dylib. Every hop below is this binary
 * or plain fork/exec with explicit envp surgery (stack arrays,
 * no allocator: some hops run in fork children of a
 * multithreaded process where malloc can abort).
 *
 *   root (default):
 *     1. probe the denied path (the denial event registers the
 *        agent; WELCOME stamps RETRACE_SESSION into the env)
 *     2. fork a plain child P: same image, same env -- P's
 *        agent re-HELLOs (atfork reset) with the token
 *     3. fork child S: probe (registers, session'd), then exec
 *        --leaf with RETRACE_SESSION dropped from the envp --
 *        a tokenless leaf under a TRACED, REGISTERED parent:
 *        the daemon inherits S's session and records the scrub
 *     4. fork child H: exec --hop with the PRELOAD var dropped
 *        -- the hop runs UNTRACED, restores the preload, and
 *        FORKS a traced --leaf: the leaf's parent pid is the
 *        unregistered hop -- a true hole; the session token
 *        survived the drop and rides through
 *
 *   --leaf: probe, sleep, exit.
 *   --hop <lib>: untraced; restore preload to <lib>; fork; the
 *     child execs --leaf.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

extern char **environ;

static const char *preload_var(void)
{
#ifdef __APPLE__
	return "DYLD_INSERT_LIBRARIES";
#else
	return "LD_PRELOAD";
#endif
}

static int probe_denied(void)
{
	int fd = open("/etc/hosts", O_RDONLY);

	if (fd >= 0)
		close(fd);
	return fd;
}

/*
 * environ minus one variable, on the caller's stack; no
 * allocator. var_eq arrives as "NAME=" so prefix matching cannot
 * clip lookalike names.
 */
static char **envp_without(const char *var_eq, char **buf,
	size_t cap)
{
	size_t vlen = strlen(var_eq) - 1; /* without the '=' */
	size_t n = 0;
	char **e;

	for (e = environ; *e != NULL && n + 1 < cap; e++) {
		if (strncmp(*e, var_eq, vlen) == 0 && (*e)[vlen] == '=')
			continue;
		buf[n++] = *e;
	}
	buf[n] = NULL;
	return buf;
}

static void run_leaf(void)
{
	printf("stage:leaf denied=%d\n", probe_denied());
	fflush(stdout);
	/* the fresh image's agent needs constructor + settle (250ms)
	 * + connect + HELLO; loaded CI runners need a wider window
	 * than the root because the leaf's whole life is this sleep
	 */
	sleep(5);
}

static void run_hop(const char *self, const char *lib)
{
	pid_t c;

	/* untraced here (the preload was dropped for this exec):
	 * setenv and the allocator are safe
	 */
	setenv(preload_var(), lib, 1);
	c = fork();
	if (c == 0) {
		execl(self, self, "--leaf", NULL);
		_exit(1);
	}
	waitpid(c, NULL, 0);
}

int main(int argc, char **argv)
{
	if (argc > 2 && strcmp(argv[1], "--hop") == 0) {
		run_hop(argv[0], argv[2]);
		return 0;
	}
	if (argc > 1 && strcmp(argv[1], "--leaf") == 0) {
		run_leaf();
		return 0;
	}

	printf("stage:root denied=%d\n", probe_denied());
	fflush(stdout);
	/* let the root's agent connect and stamp RETRACE_SESSION
	 * before forking (children inherit the env at fork)
	 */
	sleep(1);

	/* P: plain fork child -- edge + session via env */
	{
		pid_t c = fork();

		if (c == 0) {
			printf("stage:fork-child denied=%d\n",
				probe_denied());
			fflush(stdout);
			sleep(1);
			_exit(0);
		}
		waitpid(c, NULL, 0);
	}

	/* S: registers, then execs tokenless -- the scrub case */
	{
		pid_t c = fork();

		if (c == 0) {
			char *envp[512];
			int drc = probe_denied();

			printf("stage:s-child denied=%d\n", drc);
			fflush(stdout);
			/* let the agent register BEFORE the exec tears
			 * its connection down (CI runners need the
			 * wider window)
			 */
			sleep(5);
			execve(argv[0],
				(char *[]){argv[0], (char *)"--leaf",
					NULL},
				envp_without("RETRACE_SESSION=", envp,
					512));
			_exit(1);
		}
		waitpid(c, NULL, 0);
	}

	/* H: untraced hop -- the hole case */
	{
		pid_t c = fork();
		char var[64];
		char *envp[512];
		const char *lib = getenv(preload_var());

		snprintf(var, sizeof(var), "%s=", preload_var());
		if (c == 0) {
			execve(argv[0],
				(char *[]){argv[0], (char *)"--hop",
					lib != NULL ? lib : "", NULL},
				envp_without(var, envp, 512));
			_exit(1);
		}
		waitpid(c, NULL, 0);
	}

	probe_denied();
	printf("stage:root-done\n");
	fflush(stdout);
	return 0;
}
