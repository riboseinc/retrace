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

#include "spawn.h"

#define RTR_SPAWN_LOG(...)           { if (!spawn_opt->silent) printf(__VA_ARGS__); }

/*
 * free command list
 */

static void free_cmd_list(struct rtr_spawn_opt *spawn_opt)
{
	int i;

	/* free command buffer and list */
	if (!spawn_opt->cmd_list)
		return;

	for (i = 0; i < spawn_opt->num_of_cmds; i++)
		free(spawn_opt->cmd_list[i].cmd);

	free(spawn_opt->cmd_list);
}

/*
 * init spawn
 */

int rtr_spawn_init(int timeout, int num_of_forks, const char *cmd_file, int silent, struct rtr_spawn_opt *spawn_opt)
{
	FILE *cmd_fp;

	/* init options */
	memset(spawn_opt, 0, sizeof(struct rtr_spawn_opt));
	spawn_opt->timeout = timeout;
	spawn_opt->silent = silent;

	/* get command list from file */
	cmd_fp = fopen(cmd_file, "r");
	if (!cmd_fp) {
		RTR_SPAWN_LOG("Could not open command file '%s' for reading\n", cmd_file);
		return -1;
	}

	while (!feof(cmd_fp)) {
		char *line = NULL;
		size_t len = 0;
		ssize_t ret;

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
		spawn_opt->cmd_list = (struct spawn_cmd *) realloc(spawn_opt->cmd_list,
								(spawn_opt->num_of_cmds + 1) * sizeof(struct spawn_cmd));
		if (!spawn_opt->cmd_list) {
			RTR_SPAWN_LOG("Out of memory!\n");
			free(line);
			break;
		}

		memset(&spawn_opt->cmd_list[spawn_opt->num_of_cmds], 0, sizeof(struct spawn_cmd));
		spawn_opt->cmd_list[spawn_opt->num_of_cmds++].cmd = line;
	}
	fclose(cmd_fp);

	/* check count of commands */
	if (spawn_opt->num_of_cmds == 0) {
		RTR_SPAWN_LOG("Invalid command file '%s'\n", cmd_file);
		return -1;
	}

	/* get forks count */
	if (num_of_forks == -1 || num_of_forks > spawn_opt->num_of_cmds)
		spawn_opt->num_of_forks = spawn_opt->num_of_cmds;
	else
		spawn_opt->num_of_forks = num_of_forks;

	/* create thread to fork commands */
	spawn_opt->fork_list = (struct fork_info *) malloc(spawn_opt->num_of_forks * sizeof(struct fork_info));
	if (!spawn_opt->fork_list) {
		RTR_SPAWN_LOG("Out of memory!\n");
		free_cmd_list(spawn_opt);
		return -1;
	}
	memset(spawn_opt->fork_list, 0, spawn_opt->num_of_forks * sizeof(struct fork_info));

	/* init mutex */
	pthread_mutex_init(&spawn_opt->mt, NULL);

	return 0;
}

/*
 * fork new process with command
 */

static int fork_cmd(struct rtr_spawn_opt *spawn_opt, struct fork_info *fi)
{
	pid_t pid;
	int i;

	char *cmd = NULL;

	char *tok, **args = NULL;
	int tok_count = 0;

	int ret = 1;

	pthread_mutex_lock(&spawn_opt->mt);

	/* check if command to be forked is exist */
	for (i = 0; i < spawn_opt->num_of_cmds; i++) {
		if (!spawn_opt->cmd_list[i].status) {
			cmd = strdup(spawn_opt->cmd_list[i].cmd);
			fi->cmd = spawn_opt->cmd_list[i].cmd;
			spawn_opt->cmd_list[i].status = 1;
			break;
		}
	}

	pthread_mutex_unlock(&spawn_opt->mt);

	if (!cmd) {
		RTR_SPAWN_LOG("No command to be executed at thread[#%d].\n", fi->index);
		return 0;
	}

	RTR_SPAWN_LOG("Running command '%s' at thread[#%d]\n", fi->cmd, fi->index);

	/* parse command arguments */
	tok = strtok(cmd, " ");
	if (!tok) {
		RTR_SPAWN_LOG("Invalid command '%s' at thread[#%d],\n", fi->cmd, fi->index);
		return -1;
	}

	while (tok) {
		args = realloc(args, (tok_count + 2) * sizeof(char *));
		if (!args) {
			RTR_SPAWN_LOG("Out of memory.\n");
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
		RTR_SPAWN_LOG("Couldn't fork new process to execute command '%s'\n", fi->cmd);
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

static void wait_for_cmd(struct rtr_spawn_opt *spawn_opt, struct fork_info *fi)
{
	int timeout = 0;

	do {
		int w, status;

		/* wait until process has terminated */
		w = waitpid(fi->pid, &status, WNOHANG);
		if (w < 0) {
			RTR_SPAWN_LOG("waitpid() error for command '%s' at thread[#%d]\n",
					fi->cmd, fi->index);
			break;
		} else if (w == 0) {
			if (timeout >= spawn_opt->timeout) {
				RTR_SPAWN_LOG("The command '%s' was killed (timeout at thread[#%d])\n",
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
			RTR_SPAWN_LOG("The command '%s' has exited with status '%d' at thread[#%d]\n",
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
	struct rtr_spawn_opt *spawn_opt = fi->spawn_opt;

	RTR_SPAWN_LOG("Started forking thread[#%d]\n", fi->index);

	while (1) {
		char *cmd;
		int ret;

		pid_t pid;

		/* fork command */
		ret = fork_cmd(spawn_opt, fi);
		if (ret == 0)             /* no left command */
			break;
		else if (ret < 0)         /* fork() failed */
			continue;

		/* wait until command has been terminated or for timeout */
		wait_for_cmd(spawn_opt, fi);
	}

	RTR_SPAWN_LOG("Ended forking thread[#%d]\n", fi->index);

	return 0;
}

/*
 * run spawn
 */

int rtr_spawn_run(struct rtr_spawn_opt *spawn_opt)
{
	int i;

	/* create threads to fork commands */
	for (i = 0; i < spawn_opt->num_of_forks; i++) {
		struct fork_info *fi = &spawn_opt->fork_list[i];
		int ret;

		fi->index = i;
		fi->spawn_opt = spawn_opt;
		if (pthread_create(&fi->th, NULL, fork_cmd_proc, (void *)fi) != 0) {
			RTR_SPAWN_LOG("pthread_create() failed with fork(#%d).\n", i);
			fi->th = -1;
		}
	}

	return 0;
}

/*
 * free spawn options
 */

int rtr_spawn_finalize(struct rtr_spawn_opt *spawn_opt)
{
	int i;

	/* wait until forking threads has been terminated */
	for (i = 0; i < spawn_opt->num_of_forks; i++) {
		if (spawn_opt->fork_list[i].th < 0)
			continue;

		pthread_join(spawn_opt->fork_list[i].th, NULL);
	}

	/* free forking info list */
	free(spawn_opt->fork_list);

	/* destroy mutex */
	pthread_mutex_destroy(&spawn_opt->mt);

	/* free command list */
	free_cmd_list(spawn_opt);
}
