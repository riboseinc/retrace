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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "spawn.h"

extern char *__progname;

/*
 * show usage
 */

static void usage(void)
{
	fprintf(stderr, "Usage: %s [options]\n"
					"\t-t <timeout in seconds> The timeout in seconds(default:60s)\n"
					"\t-n <number of forks>    The count of concurrent forks processes\n"
					"\t-c <command file>       The file which holds command list\n"
					"\t-s                      Enable silent mode\n",
						__progname);
	exit(-1);
}

/*
 * main function
 */

int main(int argc, char *argv[])
{
	struct rtr_spawn_opt spawn_opt;

	int timeout = RTR_SPAWN_DEFAULT_TIMEOUT;
	int num_of_forks = -1;
	const char *cmd_list_file = NULL;
	int enable_silent = 0;

	int opt;

	/* parse arguments */
	if (argc < 2)
		usage();

	while ((opt = getopt(argc, argv, "t:n:c:s")) != -1) {
		switch (opt) {
		case 't':
		{
			timeout = (int)strtoul(optarg, NULL, 10);
			if (timeout > RTR_SPAWN_MAX_TIMEOUT) {
				fprintf(stderr, "Invalid timeout value '%d'. Please specify a value in [0 ~ %d] seconds.\n",
						timeout, RTR_SPAWN_MAX_TIMEOUT);
				exit(1);
			}
			break;
		}

		case 'n':
			num_of_forks = (int)strtoul(optarg, NULL, 10);
			break;

		case 'c':
			cmd_list_file = optarg;
			break;

		case 's':
			enable_silent = 1;
			break;

		default:
			usage();
			break;
		}
	}

	if (!cmd_list_file) {
		fprintf(stderr, "Please specify the command list file.\n");
		exit(1);
	}

	/* initialize spawn */
	if (rtr_spawn_init(timeout, num_of_forks, cmd_list_file, enable_silent, &spawn_opt) != 0) {
		fprintf(stderr, "Failed to initialize spawn options.\n");
		exit(1);
	}

	/* spawn commands */
	rtr_spawn_run(&spawn_opt);

	/* finalize spawn */
	rtr_spawn_finalize(&spawn_opt);

	return 0;
}
