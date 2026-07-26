
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
#include <unistd.h>

#include <nereon.h>
#include "retrace_v2.h"
#include "retrace_v2.nos.h"

static int g_prog_exit;

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
	fprintf(stderr, "Received signal '%d'\n", signum);
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
 * free bin arguments
 */

static void
free_bin_args(char **bin_args, int bin_args_count)
{
	int i;

	if (bin_args_count == 0)
		return;

	for (i = 0; i < bin_args_count; i++)
		free(bin_args[i]);
	free(bin_args);
}

/*
 * free options
 */

static void
free_options(struct rtr2_options *opt)
{
	if (opt->bin_path)
		free(opt->bin_path);

	if (opt->bin_args)
		free_bin_args(opt->bin_args, opt->bin_args_count);
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

		snprintf(path, sizeof(path), "%s/%s", g_syslib_dirs[i], RTR2_LIB_NAME);
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
check_binary_avail(struct rtr2_options *opt)
{
	char *tok, *brkb = NULL;
	struct stat st;

	for (tok = strtok_r(opt->proc_cli, " ", &brkb); tok; tok = strtok_r(NULL, " ", &brkb)) {
		if (opt->bin_args_count == 0)
			opt->bin_path = strdup(tok);

		opt->bin_args = (char **)realloc(opt->bin_args, (opt->bin_args_count + 1) * sizeof(char **));
		if (!opt->bin_args)
			break;

		opt->bin_args[opt->bin_args_count++] = strdup(tok);
	}

	/* get path of binary */
	if (!strchr(opt->bin_path, '/')) {
		char *sys_path, *bin_full_path = NULL;

		/* try to find in system path */
		sys_path = getenv("PATH");
		if (!sys_path)
			return -1;

		for (tok = strtok_r(sys_path, ":", &brkb); tok; tok = strtok_r(NULL, " ", &brkb)) {
			bin_full_path = (char *) malloc(strlen(tok) + strlen(opt->bin_path) + 2);
			if (!bin_full_path)
				return -1;

			memset(bin_full_path, 0, strlen(tok) + strlen(opt->bin_path) + 2);
			if (tok[strlen(tok) - 1] == '/')
				strcpy(bin_full_path, tok);
			else {
				strcpy(bin_full_path, tok);
				strcat(bin_full_path, "/");
			}
			strcat(bin_full_path, opt->bin_path);

			/* check if path is exist */
			if (stat(bin_full_path, &st) == 0) {
				free(opt->bin_path);
				opt->bin_path = bin_full_path;
				break;
			}
			free(bin_full_path);
			bin_full_path = NULL;
		}
	}

	if (!opt->bin_path)
		return -1;

	/* check avaiability of binary */
	if (stat(opt->bin_path, &st) != 0) {
		fprintf(stderr, "Coudln't find binary '%s' in system\n", opt->bin_path);
		goto end;
	}

	if ((st.st_mode & S_IXUSR) && check_executable(opt->bin_path, &st) == 0)
		return 0;

end:

	return -1;
}

/*
 * fork command
 */

static pid_t
fork_cmd(struct rtr2_options *opt)
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
			setenv("RETRACE_V2_CONFIG", opt->config_path, 1);

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
check_options(struct rtr2_options *opt)
{
	/* find library path */
	if (!opt->lib_path) {
		opt->sys_lib_path = find_lib_in_sys();
		opt->lib_path = opt->sys_lib_path;
	}

	if (!opt->lib_path || check_dynamic_loadable(opt->lib_path) != 0) {
		fprintf(stderr, "Please specify valid path of %s library file\n", RTR2_LIB_NAME);
		return -1;
	}

	/* check avaiability of command */
	if (check_binary_avail(opt) < 0) {
		fprintf(stderr, "Invalid command '%s'\n", opt->proc_cli);
		return -1;
	}

	return 0;
}

/*
 * trace command
 */

static void
trace_cmd(struct rtr2_options *opt)
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
	nereon_ctx_t ctx;
	int ret;
	bool require_exit = false;

	struct rtr2_options opt;

	struct nereon_config_option cfg_opts[] = {
		{"config_file", NEREON_TYPE_CONFIG, false, NULL, &opt.config_path},
		{"proc_cmdline", NEREON_TYPE_STRING, true, NULL, &opt.proc_cli},
		{"lib_path", NEREON_TYPE_STRING, false, NULL, &opt.lib_path},
		{"verbose", NEREON_TYPE_INT, false, NULL, &opt.verbose},
		{"log_path", NEREON_TYPE_STRING, false, NULL, &opt.log_path},
		{"print_version", NEREON_TYPE_BOOL, false, NULL, &opt.print_version},
	};

	memset(&opt, 0, sizeof(struct rtr2_options));

	/* initialize nereon context */
	ret = nereon_ctx_init(&ctx, get_rtr2_nos_cfg());
	if (ret != 0) {
		fprintf(stderr, "Could not initialize nereon context(err:%s)\n", nereon_get_errmsg());
		exit(1);
	}

	/* print command line usage */
	ret = nereon_parse_cmdline(&ctx, argc, argv, &require_exit);
	if (ret != 0 || require_exit) {
		if (ret != 0)
			fprintf(stderr, "Failed to parse command line(err:%s)\n", nereon_get_errmsg());

		nereon_print_usage(&ctx);
		nereon_ctx_finalize(&ctx);

		exit(ret);
	}

	ret = nereon_get_config_options(&ctx, cfg_opts);
	if (ret != 0) {
		fprintf(stderr, "Failed to get configuration options(err:%s)\n", nereon_get_errmsg());
		goto end;
	}

	if (opt.print_version) {
		nereon_ctx_finalize(&ctx);
		print_version();
	}

	ret = check_options(&opt);
	if (ret != 0)
		goto end;

	/* init signal */
	init_signal();

	/* trace command */
	trace_cmd(&opt);

end:
	/* finalize nereon context */
	nereon_ctx_finalize(&ctx);

	/* free allocated memory for options */
	free_options(&opt);

	return ret;
}
