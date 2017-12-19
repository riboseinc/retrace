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
#include <getopt.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

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
	fprintf(stderr, "Usage: retrace [options] <command line>\n"
					"[options]:\n"
					"\t--lib <library path>     The path of '%s' library\n"
					"\t--config <config path>   The path of configuration\n",
					RETRACE_LIB_NAME);
	exit(1);
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
 * try to find libretrace.so in system path
 */

static const char *g_syslib_dirs[] = {
	"/usr/local/lib",
	"/usr/lib",
	NULL
};

static const char *
find_lib(const char *lib_path)
{
	void *handle;
	int i;

	static char path[MAX_PATH];
	int find_lib = 1;

	memset(path, 0, sizeof(path));

	if (lib_path) {
		if (strlen(lib_path) > MAX_PATH - 1)
			strncpy(path, lib_path, MAX_PATH - 1);
		else
			strcpy(path, lib_path);
	} else {
		find_lib = 0;
		for (i = 0; g_syslib_dirs[i] != NULL; i++) {
			struct stat st;

			snprintf(path, sizeof(path), "%s/%s", g_syslib_dirs[i], RETRACE_LIB_NAME);
			if (stat(path, &st) == 0) {
				find_lib = 1;
				break;
			}
		}
	}

	if (!find_lib)
		return NULL;

	printf("Try to load '%s'... ", path);

	handle = dlopen(path, RTLD_NOW);
	if (!handle) {
		printf("Failed\n");
		return NULL;
	}

	printf("Success\n");
	dlclose(handle);

	return path;
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
check_binary_avail(const char *cmd_name, char **_bin_path)
{
	char *bin_path;
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
		}
	} else
		bin_path = strdup(cmd_name);

	/* check avaiability of binary */
	if (stat(bin_path, &st) != 0) {
		fprintf(stderr, "Coudln't find binary '%s' in system\n", cmd_name);
		return -1;
	}

	if ((st.st_mode & S_IXUSR) &&
		 check_executable(bin_path, &st) == 0) {
		*_bin_path = bin_path;
		return 0;
	}

	return -1;
}

/*
 * fork command
 */

static pid_t
fork_cmd(const char *lib_path, const char *config_path, const char *bin_path, char **cmd_args)
{
	pid_t pid;

	pid = fork();
	if (pid == 0) {
		/* set environments */
#ifdef __APPLE__
		setenv("DYLD_FORCE_FLAG_NAMESPACE", "1", 1);
		setenv("DYLD_INSERT_LIBRARIES", lib_path, 1);
#else
		setenv("LD_PRELOAD", lib_path, 1);
#endif
		if (config_path)
			setenv("RETRACE_CONFIG", config_path, 1);

		execv(bin_path, cmd_args);
		exit(1);
	} else if (pid < 0)
		return -1;

	return pid;
}

/*
 * main function
 */

int
main(int argc, char *argv[])
{
	const char *lib_path = NULL;
	const char *config_path = NULL;

	char *bin_path = NULL;
	int i = 1;

	pid_t child_pid;
	int w, status;

	/* parse arguments */
	if (argc < 2)
		usage();

	do {
		if (strcmp(argv[i], "--lib") == 0)
			lib_path = argv[++i];
		else if (strcmp(argv[i], "--config") == 0)
			config_path = argv[++i];
		else
			break;
	} while (1);

	/* find library path */
	lib_path = find_lib(lib_path);
	if (!lib_path) {
		fprintf(stderr, "Please specify valid path of %s library file\n", RETRACE_LIB_NAME);
		exit(1);
	}

	/* check avaiability of command */
	if (check_binary_avail(argv[i], &bin_path) < 0) {
		fprintf(stderr, "Invalid command '%s'\n", argv[i]);
		exit(1);
	}

	/* init signal */
	init_signal();

	/* fork command */
	child_pid = fork_cmd(lib_path, config_path, bin_path, &argv[i]);
	if (child_pid > 0) {
		free(bin_path);
		exit(1);
	}

	do {
		w = waitpid(-1, &status, WNOHANG);
		if (w < 0)
			break;

		if (g_prog_exit)
			kill(-1, SIGTERM);
	} while (!WIFEXITED(status) && !WIFSIGNALED(status));

	free(bin_path);

	return 0;
}
