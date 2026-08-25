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
 * Policy-distribution target (TODO.supervisor/05): loops for
 * argv[1] seconds, each iteration probing three paths -- the
 * denied one, the allowed one, and a UNIQUE creatable one.
 * One line per iteration, flushed, so the supervisor E2E can
 * watch the policy land LIVE:
 *
 *   iter=<n> denied=<rc> allowed=<rc> creatable=<rc>
 *
 * The creatable probe is the kernel truth: a real open(O_CREAT)
 * leaves a file behind; a frozen/suppressed one does not.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int seconds = argc > 1 ? atoi(argv[1]) : 6;
	const char *denied = argc > 2 ? argv[2] : "/etc/hosts";
	const char *allowed = argc > 3 ? argv[3] : "/etc/protocols";
	const char *creatable_dir = argc > 4 ? argv[4] : "/tmp";
	long iter = 0;
	time_t end = time(NULL) + seconds;

	for (;;) {
		time_t now = time(NULL);

		/*
		 * Under a wildcard freeze even time() is frozen
		 * (synthesized -1): stay alive on the zero clock --
		 * the supervisor still owns the process, and the
		 * exit decision is not the specimen's to make.
		 */
		if (now == (time_t)-1 || now < 1700000000)
			now = 0;
		if (now >= end)
			break;

		{
			char path[512];
			int a, b, c;

			a = open(denied, O_RDONLY);
			if (a >= 0)
				close(a);

			b = open(allowed, O_RDONLY);
			if (b >= 0)
				close(b);

			snprintf(path, sizeof(path),
				"%s/sup-pol-%ld-%ld",
				creatable_dir, (long)getpid(), iter);
			c = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
			if (c >= 0)
				close(c);

			printf("iter=%ld denied=%d allowed=%d creatable=%d\n",
				iter, a, b, c);
			fflush(stdout);
			iter++;
			sleep(1);
		}
	}
	return 0;
}
