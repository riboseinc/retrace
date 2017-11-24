#ifndef __SPAWN_H__
#define __SPAWN_H__

#define RTR_SPAWN_MAX_TIMEOUT              3600
#define RTR_SPAWN_DEFAULT_TIMEOUT          60

struct rtr_spawn_opt;

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

	struct rtr_spawn_opt *spawn_opt;
};

struct rtr_spawn_opt {
	int timeout;

	int num_of_cmds;
	struct spawn_cmd *cmd_list;

	int num_of_forks;
	struct fork_info *fork_list;

	int silent;
	pthread_mutex_t mt;
};

/*
 * spawn API functions
 */

int rtr_spawn_init(int timeout, int num_of_forks, const char *cmd_file, int silent, struct rtr_spawn_opt *spawn_opt);
int rtr_spawn_run(struct rtr_spawn_opt *spawn_opt);
void rtr_spawn_finalize(struct rtr_spawn_opt *spawn_opt);

#endif // __SPAWN_H__
