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

#ifdef __linux__

#include <linux/limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "retrace_cli.h"

#define RETRACE_VER "0.2"
#define RETRACE_ENV_CLI_ENA "RETRACE_CLI"
#define RETRACE_INFO_DELAY 3

#define retrace_init_info(fmt, ...) printf("RETRACE-INIT [INFO]: " fmt, ## __VA_ARGS__)
#define retrace_init_err(fmt, ...) printf("RETRACE-INIT [ERROR]: " fmt, ## __VA_ARGS__)
#define retrace_init_dbg(fmt, ...) printf("RETRACE-INIT [DBG]: " fmt, ## __VA_ARGS__)

/**
 *  @brief Initializes retrace upon loading into process address space
 */

static void cmd_func_hello(void)
{
	cli_printf("Hello\r\n");
}

static void cmd_func_exit(void)
{
	exit(1);
}

static cli_cmd_t cmd_blk[] = {
		{"Say Hi", cmd_func_hello},
		{"Terminate process", cmd_func_exit},
		{"", NULL}
};

/* this function is run by the second thread */
static void *cli_thread(void *x_void_ptr)
{
	cli_run();
	return NULL;
}

__attribute__((constructor))
static void retrace_main(void)
{
	char *cli_env;
	char pts_path[PATH_MAX];
	int i;
	pthread_t cli_t;

	cli_env = getenv(RETRACE_ENV_CLI_ENA);
	if ((cli_env != NULL) && (atoi(cli_env) == 1)) {
		/* output only if not in silent mode */
		retrace_init_info("version: %s\n", RETRACE_VER);

		/* init cli */
		if (cli_init(pts_path, PATH_MAX)) {
			retrace_init_err("failed to init cli!\n");
			return;
		}

		retrace_init_info("cli pts is at: %s\n", pts_path);

		/* register commands */
		if (cli_register_command_blk(cmd_blk)) {
			retrace_init_err("failed to register commands!\n");
			return;
		}

		/* create cli thread */
		if (pthread_create(&cli_t, NULL, cli_thread, NULL)) {
			retrace_init_err("failed to create cli thread!\n");
			return;
		}

		for (i = RETRACE_INFO_DELAY; i; i--) {
			retrace_init_info("continuing in %u sec...\n", i);

			sleep(1);
		}
	}
}

#endif /* __linux__ */

