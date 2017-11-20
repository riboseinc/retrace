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
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#define MAX_TIMEOUT                    60          /* 60 seconds */

/*
 * spawn context structure
 */

struct spawn_cmd {
	char *cmd;
	int status;                       /* 0 - not forked, 1 - forked */
};

struct fork_info {
	int index;
	pthread_t th;

	pid_t pid;
	const char *cmd;
};

struct spawn_opt {
	int timeout;

	int num_of_cmds;
	struct spawn_cmd *cmd_list;

	int num_of_forks;
	struct fork_info *fork_list;

	pthread_mutex_t mt;
};

static struct spawn_opt g_opt;

/*
 * show usage
 */

static void usage(const char *prog_name)
{
	fprintf(stderr, "Usage: %s <timeout in seconds> <number of forks> <command file>\n",
						prog_name);
	exit(-1);
}

/*
 * parse options
 */

static int parse_options(const char *timeout, const char *num_of_forks, const char *cmd_file)
{
	FILE *cmd_fp;

	/* init options */
	memset(&g_opt, 0, sizeof(g_opt));

	/* get timeout */
	g_opt.timeout = (int)strtoul(timeout, NULL, 10);
	if (g_opt.timeout < 0 || g_opt.timeout > 60) {
		fprintf(stderr, "Invalid timeout value '%d'. Please specify a value in [0 ~ 60] seconds.\n",
				g_opt.timeout);
		return -1;
	}

	/* get command list from file */
	cmd_fp = fopen(cmd_file, "r");
	if (!cmd_fp) {
		fprintf(stderr, "Could not open command file '%s' for reading\n", cmd_file);
		exit(1);
	}

	while (!feof(cmd_fp)) {
		char *line = NULL;
		size_t len = 0, ret;

		/* get command line */
		ret = getline(&line, &len, cmd_fp);
		if (ret < 0) {
			if (line)
				free(line);
			break;
		}

		/* chop endline */
		if (line[strlen(line) - 1] == '\n')
			line[strlen(line) - 1] = '\0';

		/* check the length of line and comment */
		if (strlen(line) == 0 || line[0] == '#') {
			free(line);
			continue;
		}

		/* add new command to list */
		g_opt.cmd_list = (struct spawn_cmd *) realloc(g_opt.cmd_list,
								(g_opt.num_of_cmds + 1) * sizeof(struct spawn_cmd));
		if (!g_opt.cmd_list) {
			fprintf(stderr, "Out of memory!\n");
			free(line);
			break;
		}

		memset(&g_opt.cmd_list[g_opt.num_of_cmds], 0, sizeof(struct spawn_cmd));
		g_opt.cmd_list[g_opt.num_of_cmds++].cmd = line;
	}

	fclose(cmd_fp);

	/* check count of commands */
	if (g_opt.num_of_cmds == 0) {
		fprintf(stderr, "Invalid command file '%s'\n", cmd_file);
		exit(1);
	}

	/* get forks count */
	g_opt.num_of_forks = strtoul(num_of_forks, NULL, 10);
	if (g_opt.num_of_forks > g_opt.num_of_cmds)
		g_opt.num_of_forks = g_opt.num_of_cmds;

	/* init mutex */
	pthread_mutex_init(&g_opt.mt, NULL);

	return 0;
}

/*
 * finalize options
 */

static void finalize_options(void)
{
	int i;

	/* wait until forking threads has been terminated */
	for (i = 0; i < g_opt.num_of_forks; i++) {
		if (g_opt.fork_list[i].th < 0)
			continue;

		pthread_join(g_opt.fork_list[i].th, NULL);
	}

	/* free forking info list */
	free(g_opt.fork_list);

	/* destroy mutex */
	pthread_mutex_destroy(&g_opt.mt);

	/* free command buffer and list */
	if (!g_opt.cmd_list)
		return;

	for (i = 0; i < g_opt.num_of_cmds; i++)
		free(g_opt.cmd_list[i].cmd);

	free(g_opt.cmd_list);
}

/*
 * fork new process with command
 */

static int fork_cmd(struct fork_info *fi)
{
	pid_t pid;
	int i;

	char *cmd = NULL;

	char *tok, **args = NULL;
	int tok_count = 0;

	int ret = 1;

	pthread_mutex_lock(&g_opt.mt);

	/* check if command to be forked is exist */
	for (i = 0; i < g_opt.num_of_cmds; i++) {
		if (!g_opt.cmd_list[i].status) {
			cmd = strdup(g_opt.cmd_list[i].cmd);
			fi->cmd = g_opt.cmd_list[i].cmd;
			g_opt.cmd_list[i].status = 1;
			break;
		}
	}

	pthread_mutex_unlock(&g_opt.mt);

	if (!cmd) {
		fprintf(stderr, "No left command to be executed at thread[#%d].\n", fi->index);
		return 0;
	}

	fprintf(stderr, "Running command '%s' at thread[#%d]\n", fi->cmd, fi->index);

	/* parse command arguments */
	tok = strtok(cmd, " ");
	if (!tok) {
		fprintf(stderr, "Invalid command line '%s' at thread[#%d],\n", fi->cmd, fi->index);
		return -1;
	}

	while (tok) {
		args = realloc(args, (tok_count + 2) * sizeof(char *));
		if (!args) {
			fprintf(stderr, "Out of memory.\n");
			free(cmd);

			return -1;
		}

		args[tok_count] = tok;
		tok_count++;

		tok = strtok(NULL, " ");
	}
	args[tok_count] = NULL;

	/* fork new process */
	pid = fork();
	if (pid == 0) {
		int fd_null;

		/* open /dev/null for redirecting output */
		fd_null = open("/dev/null", O_WRONLY);
		if (fd_null > 0) {
			dup2(fd_null, STDOUT_FILENO);
			dup2(fd_null, STDERR_FILENO);

			close(fd_null);
		}

		exit(execvp(args[0], args));
	} else if (pid < 0) {
		fprintf(stderr, "Couldn't fork new process to execute command '%s'\n", fi->cmd);
		ret = -1;
	}

	free(args);
	free(cmd);

	fi->pid = pid;

	return ret;
}

/*
 * wait for command has finished or timeout
 */

static void wait_for_cmd(struct fork_info *fi)
{
	int timeout = 0;

	do {
		int w, status;

		/* wait until process has terminated */
		w = waitpid(fi->pid, &status, WNOHANG);
		if (w < 0) {
			fprintf(stderr, "waitpid() error for command '%s' at thread[#%d]\n",
					fi->cmd, fi->index);
			break;
		} else if (w == 0) {
			if (timeout >= g_opt.timeout) {
				fprintf(stderr, "The command '%s' has killed by timeout at thread[#%d]\n",
						fi->cmd, fi->index);
				kill(fi->pid, SIGTERM);
				break;
			}
			timeout++;

			sleep(1);
			continue;
		}

		/* set exit status */
		if (WIFEXITED(status)) {
			fprintf(stderr, "The command '%s' has exited with status '%d' at thread[#%d]\n",
					fi->cmd, WEXITSTATUS(status), fi->index);
			break;
		}
	} while (1);
}

/*
 * thread for forking command
 */

static void *fork_cmd_proc(void *p)
{
	struct fork_info *fi = (struct fork_info *)p;

	fprintf(stderr, "Started forking thread[#%d]\n", fi->index);

	while (1) {
		char *cmd;
		int ret;

		pid_t pid;

		/* fork command */
		ret = fork_cmd(fi);
		if (ret == 0)             /* no left command */
			break;
		else if (ret < 0)         /* fork() failed */
			continue;

		/* wait until command has been terminated or for timeout */
		wait_for_cmd(fi);
	}

	fprintf(stderr, "Ended forking thread[#%d]\n", fi->index);

	return 0;
}

/*
 * main function
 */

int main(int argc, char *argv[])
{
	int i;

	/* check arguments */
	if (argc != 4)
		usage(argv[0]);

	/* parse options */
	if (parse_options(argv[1], argv[2], argv[3]) != 0)
		exit(1);

	/* create thread to fork commands */
	g_opt.fork_list = (struct fork_info *) malloc(g_opt.num_of_forks * sizeof(struct fork_info));
	if (!g_opt.fork_list) {
		fprintf(stderr, "Out of memory!\n");
		finalize_options();

		exit(1);
	}
	memset(g_opt.fork_list, 0, g_opt.num_of_forks * sizeof(struct fork_info));

	/* create threads to fork commands */
	for (i = 0; i < g_opt.num_of_forks; i++) {
		struct fork_info *fi = &g_opt.fork_list[i];
		int ret;

		fi->index = i;
		if (pthread_create(&fi->th, NULL, fork_cmd_proc, (void *)fi) != 0) {
			fprintf(stderr, "pthread_create() failed.\n");
			fi->th = -1;
		}
	}

	/* finalize options */
	finalize_options();

	return 0;
}
