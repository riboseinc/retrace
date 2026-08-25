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
 * process forest under one supervisor and exits.
 *
 *   root (default mode):
 *     1. probe the denied path (registers via the denial event)
 *     2. fork a plain child -- same image, same env: the fork
 *        child's agent re-HELLOs (atfork reset) with the
 *        inherited session token
 *     3. fork a scrub child: wipes RETRACE_SESSION then execs
 *        itself -- a tokenless leaf under a TRACED parent (the
 *        daemon stitches it to the parent's session and records
 *        the scrub)
 *     4. execs itself through `env -u $PRELOAD` -- an UNTRACED
 *        intermediate (preload scrubbed) whose leaf re-preloads:
 *        token inherited through env, tree edge across the hole
 *
 *   --leaf: probe the denied path, sleep, exit.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int probe_denied(void)
{
	int fd = open("/etc/hosts", O_RDONLY);

	if (fd >= 0)
		close(fd);
	return fd;
}

static const char *preload_var(void)
{
#ifdef __APPLE__
	return "DYLD_INSERT_LIBRARIES";
#else
	return "LD_PRELOAD";
#endif
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "--leaf") == 0) {
		printf("stage:leaf denied=%d\n", probe_denied());
		fflush(stdout);
		sleep(1);
		return 0;
	}

	printf("stage:root denied=%d\n", probe_denied());
	fflush(stdout);
	/* let the root's agent connect and stamp RETRACE_SESSION
	 * before forking (children inherit the env at fork)
	 */
	sleep(1);

	{
		pid_t c = fork();

		if (c == 0) {
			printf("stage:fork-child denied=%d\n", probe_denied());
			fflush(stdout);
			sleep(1);
			_exit(0);
		}
		waitpid(c, NULL, 0);
	}

	{
		pid_t c = fork();

		if (c == 0) {
			/* scrub via env(1), not setenv(): setenv
			 * reallocs the environ -- an allocator call in
			 * a fork child of a multithreaded process can
			 * abort on locks held by missing threads. The
			 * preload must be re-asserted explicitly: the
			 * hop through env(1) can drop it (macOS SIP
			 * scrubbing) -- an untraced leaf registers no
			 * agent and the scrub is never observed
			 */
			char assign[4096];
			const char *pre = getenv(preload_var());

			if (pre == NULL)
				pre = "";
			snprintf(assign, sizeof(assign), "%s=%s",
				preload_var(), pre);
			execl("/usr/bin/env", "env", "-u",
				"RETRACE_SESSION", assign, argv[0],
				"--leaf", NULL);
			_exit(1);
		}
		waitpid(c, NULL, 0);
	}

	/* the untraced-intermediate hop: preload scrubbed for env(1),
	 * restored for the leaf; RETRACE_SESSION rides through
	 */
	{
		char assign[4096];
		const char *pre = getenv(preload_var());

		if (pre == NULL)
			pre = "";
		snprintf(assign, sizeof(assign), "%s=%s",
			preload_var(), pre);
		execl("/usr/bin/env", "env", "-u", preload_var(),
			assign, argv[0], "--leaf", NULL);
		perror("execl env");
		return 1;
	}
}
