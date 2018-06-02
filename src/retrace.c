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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "retrace_v2/prototypes/unistd.c"

#include "retrace.h"

#ifndef __APPLE__
#define RETRACE_LIB_NAME             "libretrace.so"
#else
#define RETRACE_LIB_NAME             "libretrace.dylib"
#endif

#define MAX_PATH                     256

static int g_prog_exit;

/*
 * show usage
 */

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [options] <command line>\n"
					"[options]:\n"
					"\t--lib <library path>     The path of '%s' library\n"
					"\t--config <config path>   The path of configuration\n"
					"\t--version                Print version\n"
					"\t--help                   Print help message\n",
					PACKAGE_NAME,
					RETRACE_LIB_NAME);
}

/*
 * print version
 */

static void
print_version(void)
{
	printf("%s - %s, Copyright 2017 Ribose Inc <https://www.ribose.com>\n",
			PACKAGE_NAME,
			PACKAGE_VERSION);
	exit(0);
}

/*
 * signal handler
 */

static void
sig_handler(const int signum)
{
	g_prog_exit = 1;
}

/*
 * init signals
 */

static void
init_signal(void)
{
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGHUP, sig_handler);
}

/*
 * free options
 */

static void
free_options(struct rtr_options *opt)
{
	if (opt->lib_path)
		free(opt->lib_path);

	if (opt->config_path)
		free(opt->config_path);

	if (opt->bin_path)
		free(opt->bin_path);
}

/*
 * try to find libretrace.so in system path
 */

static const char *g_syslib_dirs[] = {
	"/usr/local/lib",
	"/usr/lib",
	NULL
};

static char *
find_lib_in_sys()
{
	char path[MAX_PATH];
	int find_lib = 0;

	int i;

	/* try to find the path of retrace library */
	memset(path, 0, sizeof(path));
	for (i = 0; g_syslib_dirs[i] != NULL; i++) {
		struct stat st;

		snprintf(path, sizeof(path), "%s/%s", g_syslib_dirs[i], RETRACE_LIB_NAME);
		if (stat(path, &st) == 0) {
			find_lib = 1;
			break;
		}
	}
	if (!find_lib)
		return NULL;

	return strdup(path);
}

/*
 * check whether library is able to be loaded dynamically
 */

static int
check_dynamic_loadable(const char *lib_path)
{
	void *handle;

	printf("Try to load '%s'... ", lib_path);

	/* try to load library for testing purpose */
	handle = dlopen(lib_path, RTLD_NOW);
	if (!handle) {
		printf("Failed\n");
		return -1;
	}

	printf("Success\n");
	dlclose(handle);

	return 0;
}

/*
 * check executable for binary
 */

static int
check_executable(const char *bin_path, struct stat *st)
{
	/* check SUID set */
	if ((st->st_mode & S_ISUID) && getuid() != 0) {
		fprintf(stderr, "The binary '%s' has SUID bit set\n", bin_path);
		return -1;
	}

	return 0;
}

/*
 * get path of binary executable
 */

static int
check_binary_avail(const char *cmd_name, struct rtr_options *opt)
{
	char *bin_path = NULL;
	struct stat st;

	int ret = -1;

	/* get path of binary */
	if (!strchr(cmd_name, '/')) {
		char *sys_path;
		char *tok;

		/* try to find in system path */
		sys_path = getenv("PATH");
		if (!sys_path)
			return -1;

		tok = strtok(sys_path, ":");
		while (tok) {
			bin_path = (char *) malloc(strlen(tok) + strlen(cmd_name) + 2);
			if (!bin_path)
				return -1;

			memset(bin_path, 0, strlen(tok) + strlen(cmd_name) + 2);
			if (tok[strlen(tok) - 1] == '/')
				strcpy(bin_path, tok);
			else {
				strcpy(bin_path, tok);
				strcat(bin_path, "/");
			}
			strcat(bin_path, cmd_name);

			/* check if path is exist */
			if (stat(bin_path, &st) == 0)
				break;

			tok = strtok(NULL, ":");

			free(bin_path);
			bin_path = NULL;
		}
	} else
		bin_path = strdup(cmd_name);

	if (!bin_path)
		return -1;

	/* check avaiability of binary */
	if (stat(bin_path, &st) != 0) {
		fprintf(stderr, "Coudln't find binary '%s' in system\n", cmd_name);
		free(bin_path);
		return -1;
	}

	if ((st.st_mode & S_IXUSR) &&
		 check_executable(bin_path, &st) == 0) {
		opt->bin_path = bin_path;
		return 0;
	}
	free(bin_path);

	return -1;
}

/*
 * fork command
 */

static pid_t
fork_cmd(struct rtr_options *opt)
{
	pid_t pid;

	pid = fork();
	if (pid == 0) {
		/* set environments */
#ifdef __APPLE__
		setenv("DYLD_FORCE_FLAT_NAMESPACE", "1", 1);
		setenv("DYLD_INSERT_LIBRARIES", opt->lib_path, 1);
#else
		setenv("LD_PRELOAD", opt->lib_path, 1);
#endif
		if (opt->config_path)
			setenv("RETRACE_CONFIG", opt->config_path, 1);

		execv(opt->bin_path, opt->bin_args);
		exit(1);
	} else if (pid < 0)
		return -1;

	return pid;
}

/*
 * parse retrace options
 */

static int
parse_options(int argc, char *argv[], struct rtr_options *opt)
{
	int i = 1;

	/* parse options */
	if (argc < 2) {
		usage();
		exit(-1);
	}

	do {
		if (strcmp(argv[i], "--lib") == 0)
			opt->lib_path = strdup(argv[++i]);
		else if (strcmp(argv[i], "--config") == 0)
			opt->config_path = strdup(argv[++i]);
		else if (strcmp(argv[i], "--version") == 0)
			print_version();
		else if (strcmp(argv[i], "--help") == 0) {
			usage();
			exit(0);
		} else
			break;
		i++;
	} while (1);

	/* find library path */
	if (!opt->lib_path)
		opt->lib_path = find_lib_in_sys();
	if (!opt->lib_path || check_dynamic_loadable(opt->lib_path) != 0) {
		fprintf(stderr, "Please specify valid path of %s library file\n", RETRACE_LIB_NAME);
		return -1;
	}

	/* check avaiability of command */
	if (check_binary_avail(argv[i], opt) < 0) {
		fprintf(stderr, "Invalid command '%s'\n", argv[i]);
		return -1;
	}
	opt->bin_args = &argv[i];

	return 0;
}

/*
 * trace command
 */

static void
trace_cmd(struct rtr_options *opt)
{
	pid_t child_pid;
	int w, status;

	child_pid = fork_cmd(opt);
	if (child_pid < 0)
		return;

	do {
		w = waitpid(-1, &status, WNOHANG);
		if (w < 0)
			break;

		if (g_prog_exit)
			kill(-1, SIGTERM);
	} while (!WIFEXITED(status) && !WIFSIGNALED(status));
}

/*
 * main function
 */

int
main(int argc, char *argv[])
{
	struct rtr_options opt;

	/* parse arguments */
	memset(&opt, 0, sizeof(struct rtr_options));
	if (parse_options(argc, argv, &opt) != 0) {
		free_options(&opt);
		exit(1);
	}

	/* init signal */
	init_signal();

	/* trace command */
	trace_cmd(&opt);

	/* free options */
	free_options(&opt);

	return 0;
}
