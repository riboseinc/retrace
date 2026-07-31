/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace CLI launcher. Replaces the v1 shell-script launcher (ADR-0011).
 *
 * Usage:
 *   retrace run [--config FILE] [--log FILE] [--quiet] -- <command...>
 *   retrace list-functions
 *   retrace list-actions
 *   retrace validate <config.json>
 *
 * `run` spawns the target with LD_PRELOAD / DYLD_INSERT_LIBRARIES set
 * to the retrace library. The library path is auto-detected from the
 * CLI binary's own location (the library is installed alongside the
 * CLI). Alternatively, set RETRACE_LIB to override.
 *
 * `list-functions` / `list-actions` walk the prototype and action
 * registries and print names. This lets users discover what retrace
 * can intercept without reading the source.
 *
 * `validate` loads the JSON config and checks that every action_name
 * and func_name is recognized. Catches typos before launch time.
 *
 * Issue #485 / #358.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

#include "parson.h"

#ifdef __linux__
#define RETRACE_PRELOAD_ENV "LD_PRELOAD"
#define RETRACE_LIB_NAME "libretrace.so"
#elif defined(__APPLE__)
#define RETRACE_PRELOAD_ENV "DYLD_INSERT_LIBRARIES"
#define RETRACE_LIB_NAME "libretrace.dylib"
#else
#define RETRACE_PRELOAD_ENV "LD_PRELOAD"
#define RETRACE_LIB_NAME "libretrace.so"
#endif

static void usage(FILE *out)
{
	fprintf(out,
"retrace v2.1.0 -- userspace libc interceptor\n"
"\n"
"Usage:\n"
"  retrace run [OPTIONS] -- <command> [args...]\n"
"  retrace trace [funcs...] [OPTIONS] -- <command>\n"
"  retrace mock <func> <retval> [OPTIONS] -- <command>\n"
"  retrace fuzz [<func>] [--rate R] [OPTIONS] -- <command>\n"
"  retrace slow <func> [--ms N] [OPTIONS] -- <command>\n"
"  retrace list-functions\n"
"  retrace list-actions\n"
"  retrace validate <config.json>\n"
"\n"
"Quick subcommands (no JSON needed):\n"
"  retrace trace malloc,free -- /bin/ls\n"
"  retrace mock getuid 0 -- ./check-root\n"
"  retrace fuzz malloc --rate 0.1 -- ./your-program\n"
"  retrace slow open --ms 100 -- ./your-program\n"
"\n"
"Options (all subcommands):\n"
"  --config FILE     Path to JSON config (default: built-in log+call_real)\n"
"  --log FILE        Path to log output file\n"
"  --quiet           Suppress retrace log output (still runs interception)\n"
"  --lib FILE        Override the retrace library path\n"
"\n"
"Environment:\n"
"  RETRACE_JSON_CONFIG   Same as --config\n"
"  RETRACE_LOGGER_DEF_FN Same as --log\n"
"  RETRACE_LIB           Same as --lib\n"
"\n");
}

/*
 * Find the retrace shared library path. Strategy:
 *   1. RETRACE_LIB env var if set
 *   2. Look alongside this binary (same directory)
 *   3. Look in common install paths
 */
static char *find_library(char *buf, size_t bufsize)
{
	const char *env;
	char exe_path[PATH_MAX];
	char dir[PATH_MAX];
	ssize_t len;

	/* 1. Env override */
	env = getenv("RETRACE_LIB");
	if (env) {
		strncpy(buf, env, bufsize - 1);
		buf[bufsize - 1] = '\0';
		return buf;
	}

	/* 2. Alongside this binary */
#ifdef __APPLE__
	len = strlen(getenv("_")); /* Apple sets _ to the exe path in shells */
	if (len > 0 && len < (ssize_t)sizeof(exe_path)) {
		strcpy(exe_path, getenv("_"));
	} else {
		/* Fall back to argv[0] via proc_pidpath */
		return NULL;
	}
#else
	len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
	if (len < 0)
		return NULL;
	exe_path[len] = '\0';
#endif
	strncpy(dir, dirname(exe_path), sizeof(dir) - 1);
	dir[sizeof(dir) - 1] = '\0';

	snprintf(buf, bufsize, "%s/%s", dir, RETRACE_LIB_NAME);
	if (access(buf, R_OK) == 0)
		return buf;

	/* 3. Common install paths */
	const char *paths[] = {
		"/usr/local/lib/" RETRACE_LIB_NAME,
		"/usr/lib/" RETRACE_LIB_NAME,
		"/usr/lib/x86_64-linux-gnu/" RETRACE_LIB_NAME,
		"/usr/lib/aarch64-linux-gnu/" RETRACE_LIB_NAME,
		NULL
	};
	size_t i;

	for (i = 0; paths[i]; i++) {
		if (access(paths[i], R_OK) == 0) {
			strncpy(buf, paths[i], bufsize - 1);
			buf[bufsize - 1] = '\0';
			return buf;
		}
	}

	return NULL;
}

static void launch_target(const char *config, const char *logfile,
			  const char *lib_override, int quiet,
			  int argc, char **argv)
{
	char lib_path[PATH_MAX];
	char *found;

	if (lib_override)
		setenv("RETRACE_LIB", lib_override, 1);

	found = find_library(lib_path, sizeof(lib_path));
	if (!found) {
		fprintf(stderr,
			"retrace: cannot find %s. Set RETRACE_LIB or install it.\n",
			RETRACE_LIB_NAME);
		exit(1);
	}

	setenv(RETRACE_PRELOAD_ENV, lib_path, 1);

	if (config)
		setenv("RETRACE_JSON_CONFIG", config, 1);

	if (logfile) {
		setenv("RETRACE_LOGGER_DEF_FN", logfile, 1);
		setenv("RETRACE_LOGGER_DEF_STDOUT_ENA", "0", 1);
	}

	if (quiet)
		setenv("RETRACE_LOGGER_DEF_ENA", "0", 1);

	execvp(argv[0], argv);

	perror("retrace: exec failed");
	exit(127);
}

/* Find "--" in argv, return its index, or -1. */
static int find_separator(int argc, char **argv)
{
	int i;

	for (i = 0; i < argc; i++)
		if (strcmp(argv[i], "--") == 0)
			return i;
	return -1;
}

/* Write a JSON string to a temp file; return the path (static buffer). */
static const char *write_temp_config(const char *json)
{
	static char path[PATH_MAX];
	int fd;
	FILE *f;

	snprintf(path, sizeof(path), "/tmp/retrace-%d.json", (int)getpid());
	fd = mkstemp(path);
	if (fd < 0) {
		perror("retrace: mkstemp");
		return NULL;
	}
	f = fdopen(fd, "w");
	if (!f) {
		perror("retrace: fdopen");
		close(fd);
		return NULL;
	}
	fputs(json, f);
	fclose(f);
	return path;
}

/*
 * Parse common --log / --quiet / --lib options that appear after
 * subcommand-specific args and before --. Returns the index of
 * the first non-option arg (or the -- position). Writes parsed
 * values into the out-params.
 */
/*
 * Pretty-print a retrace JSON log to stdout — no Python needed.
 * Filters engine noise; shows one line per intercepted call.
 *
 * Entry types in the JSON:
 *   - message.text = "Running action ..." → engine noise, skip
 *   - message.text = "config file is ..." → config noise, skip
 *   - message.func + call_duration_us     → call summary, print
 *   - message.* (other keys)              → call args, print
 */
static void pp_log(const char *path)
{
	JSON_Value *root;
	JSON_Array *arr;
	JSON_Object *entry, *msg;
	size_t i, n;
	const char *func, *text;
	int count = 0;
	double total_us = 0;

	root = json_parse_file(path);
	if (root == NULL) {
		fprintf(stderr, "retrace: cannot parse %s\n", path);
		return;
	}

	arr = json_value_get_array(root);
	if (arr == NULL) {
		fprintf(stderr, "retrace: expected JSON array in %s\n", path);
		json_value_free(root);
		return;
	}

	n = json_array_get_count(arr);

	for (i = 0; i < n; i++) {
		entry = json_array_get_object(arr, i);
		if (entry == NULL)
			continue;

		msg = json_object_get_object(entry, "message");
		if (msg == NULL)
			continue;

		/* Skip engine noise: "Running action ...", "config file ..." */
		text = json_object_get_string(msg, "text");
		if (text != NULL)
			continue;

		/* Call summary entry: has "func" and "call_duration_us" */
		func = json_object_get_string(msg, "func");
		if (func != NULL && json_object_has_value(msg, "call_duration_us")) {
			double us = json_object_get_number(msg, "call_duration_us");
			double rv = json_object_get_number(msg, "ret_val");

			total_us += us;
			count++;

			if (us < 1.0)
				printf("  %-28s → %g  (<1µs)\n", func, rv);
			else
				printf("  %-28s → %g  (%.0fµs)\n", func, rv, us);
			continue;
		}

		/* Args entry (module ACT): print key=value pairs */
		if (func == NULL) {
			size_t j, nkeys;

			printf("    ");
			nkeys = json_object_get_count(msg);
			for (j = 0; j < nkeys; j++) {
				const char *k = json_object_get_name(msg, j);
				const char *v = json_value_get_string(
					json_object_get_value_at(msg, j));

				if (v != NULL && v[0] != '\0')
					printf("%s=%s  ", k, v);
				else
					printf("%s=?  ", k);
			}
			printf("\n");
		}
	}

	json_value_free(root);

	if (count > 0)
		printf("\n  %d calls, %.1fms total libc time\n",
		       count, total_us / 1000.0);
}

static int parse_common_opts(int argc, char **argv, int start,
			     const char **logfile, int *quiet,
			     const char **lib_override)
{
	int i;

	for (i = start; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0)
			return i;
		if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
			*logfile = argv[++i];
		} else if (strcmp(argv[i], "--quiet") == 0) {
			*quiet = 1;
		} else if (strcmp(argv[i], "--lib") == 0 && i + 1 < argc) {
			*lib_override = argv[++i];
		} else {
			return i;
		}
	}
	return i;
}

/*
 * retrace trace [funcs...] [OPTIONS] -- <command>
 *
 * If no funcs given, trace every interceptable call (wildcard).
 * Otherwise, trace only the named functions.
 */
static int cmd_trace(int argc, char **argv)
{
	const char *logfile = NULL;
	const char *lib_override = NULL;
	int quiet = 0;
	int sep, i, nfuncs = 0;
	char json[8192];
	const char *config;

	/* Count func names until we hit an option or -- */
	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-' || strcmp(argv[i], "--") == 0)
			break;
		nfuncs++;
	}

	i = parse_common_opts(argc, argv, i, &logfile, &quiet, &lib_override);
	sep = find_separator(argc, argv);
	if (sep < 0 || sep + 1 >= argc) {
		fprintf(stderr,
			"retrace trace: no command. Use: retrace trace [funcs...] -- <command>\n");
		return 1;
	}

	if (nfuncs == 0) {
		snprintf(json, sizeof(json),
			"{\"intercept_scripts\":[{\"func_name\":\"*\","
			"\"actions\":[{\"action_name\":\"log_params\"},"
			"{\"action_name\":\"call_real\"}]}]}");
	} else {
		int pos = 0;

		pos += snprintf(json + pos, sizeof(json) - pos,
			"{\"intercept_scripts\":[");
		for (i = 0; i < nfuncs; i++) {
			pos += snprintf(json + pos, sizeof(json) - pos,
				"%s{\"func_name\":\"%s\","
				"\"actions\":[{\"action_name\":\"log_params\"},"
				"{\"action_name\":\"call_real\"}]}",
				i > 0 ? "," : "", argv[i]);
		}
		pos += snprintf(json + pos, sizeof(json) - pos, "]}");
	}

	config = write_temp_config(json);
	if (!config)
		return 1;

	/*
	 * Trace mode: fork the target, capture JSON to a temp file
	 * (suppress stdout), then pretty-print the result — all in C,
	 * no external tools.
	 */
	{
		char trace_log[PATH_MAX];
		char lib_path[PATH_MAX];
		char *found;
		pid_t pid;
		int status;

		snprintf(trace_log, sizeof(trace_log),
			 "/tmp/retrace-trace-%d.json", (int)getpid());

		if (lib_override)
			setenv("RETRACE_LIB", lib_override, 1);

		found = find_library(lib_path, sizeof(lib_path));
		if (!found) {
			fprintf(stderr,
				"retrace: cannot find %s. Set RETRACE_LIB or install it.\n",
				RETRACE_LIB_NAME);
			return 1;
		}

		setenv(RETRACE_PRELOAD_ENV, lib_path, 1);
		setenv("RETRACE_JSON_CONFIG", config, 1);
		setenv("RETRACE_LOGGER_DEF_FN", trace_log, 1);
		setenv("RETRACE_LOGGER_DEF_STDOUT_ENA", "0", 1);
		if (quiet)
			setenv("RETRACE_LOGGER_DEF_ENA", "0", 1);

		/* Remove old trace log so we start fresh */
		unlink(trace_log);

		pid = fork();
		if (pid < 0) {
			perror("retrace: fork");
			return 1;
		}

		if (pid == 0) {
			/* child */
			execvp(argv[sep + 1], &argv[sep + 1]);
			perror("retrace: exec failed");
			_exit(127);
		}

		/* parent: wait for child, then pretty-print */
		while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
			;

		if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
			fprintf(stderr,
				"retrace: target exited with code %d\n",
				WEXITSTATUS(status));
		}

		/* Pretty-print the trace log */
		pp_log(trace_log);

		/* Cleanup */
		unlink(trace_log);

		if (WIFEXITED(status))
			return WEXITSTATUS(status);
		return 1;
	}
}

/*
 * retrace mock <func> <retval> [OPTIONS] -- <command>
 *
 * Make <func> always return <retval> instead of its real value.
 */
static int cmd_mock(int argc, char **argv)
{
	const char *func, *retval_str;
	const char *logfile = NULL;
	const char *lib_override = NULL;
	int quiet = 0, sep, idx;
	long retval;
	char json[1024];
	const char *config;

	if (argc < 2) {
		fprintf(stderr,
			"retrace mock: usage: retrace mock <func> <retval> -- <command>\n");
		return 1;
	}
	func = argv[0];
	retval_str = argv[1];

	retval = strtol(retval_str, NULL, 0);

	idx = parse_common_opts(argc, argv, 2, &logfile, &quiet, &lib_override);
	sep = find_separator(argc, argv);
	if (sep < 0 || sep + 1 >= argc) {
		fprintf(stderr,
			"retrace mock: no command. Use: retrace mock %s %s -- <command>\n",
			func, retval_str);
		return 1;
	}

	snprintf(json, sizeof(json),
		"{\"intercept_scripts\":[{\"func_name\":\"%s\","
		"\"actions\":[{\"action_name\":\"call_real\"},"
		"{\"action_name\":\"modify_return_value_int\","
		"\"action_params\":{\"retval_int\":%ld}}]}]}",
		func, retval);

	config = write_temp_config(json);
	if (!config)
		return 1;

	launch_target(config, logfile, lib_override, quiet,
		      argc - sep - 1, &argv[sep + 1]);
	return 0;
}

/*
 * retrace fuzz <func> [--rate R] [OPTIONS] -- <command>
 *
 * Fuzz memory allocation (or any specified func) at a configurable
 * failure rate. Default func is malloc; default rate is 0.05 (5%).
 */
static int cmd_fuzz(int argc, char **argv)
{
	const char *func = "malloc";
	double rate = 0.05;
	const char *logfile = NULL;
	const char *lib_override = NULL;
	int quiet = 0, sep, i;
	char json[1024];
	const char *config;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0)
			break;
		if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
			rate = atof(argv[++i]);
		} else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
			logfile = argv[++i];
		} else if (strcmp(argv[i], "--lib") == 0 && i + 1 < argc) {
			lib_override = argv[++i];
		} else if (strcmp(argv[i], "--quiet") == 0) {
			quiet = 1;
		} else if (argv[i][0] != '-') {
			func = argv[i];
		}
	}

	sep = find_separator(argc, argv);
	if (sep < 0 || sep + 1 >= argc) {
		fprintf(stderr,
			"retrace fuzz: no command. Use: retrace fuzz [<func>] [--rate R] -- <command>\n");
		return 1;
	}

	snprintf(json, sizeof(json),
		"{\"intercept_scripts\":[{\"func_name\":\"%s\","
		"\"actions\":[{\"action_name\":\"call_real\"},"
		"{\"action_name\":\"memory_fuzz\","
		"\"action_params\":{\"fail_rate\":%.4f}}]}]}",
		func, rate);

	config = write_temp_config(json);
	if (!config)
		return 1;

	launch_target(config, logfile, lib_override, quiet,
		      argc - sep - 1, &argv[sep + 1]);
	return 0;
}

/*
 * retrace slow <func> --ms N [OPTIONS] -- <command>
 *
 * Inject N milliseconds of latency into every call to <func>.
 */
static int cmd_slow(int argc, char **argv)
{
	const char *func = NULL;
	int ms = 100;
	const char *logfile = NULL;
	const char *lib_override = NULL;
	int quiet = 0, sep, i;
	char json[1024];
	const char *config;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0)
			break;
		if (strcmp(argv[i], "--ms") == 0 && i + 1 < argc) {
			ms = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
			logfile = argv[++i];
		} else if (strcmp(argv[i], "--lib") == 0 && i + 1 < argc) {
			lib_override = argv[++i];
		} else if (strcmp(argv[i], "--quiet") == 0) {
			quiet = 1;
		} else if (argv[i][0] != '-') {
			func = argv[i];
		}
	}

	if (func == NULL) {
		fprintf(stderr,
			"retrace slow: usage: retrace slow <func> [--ms N] -- <command>\n");
		return 1;
	}

	sep = find_separator(argc, argv);
	if (sep < 0 || sep + 1 >= argc) {
		fprintf(stderr,
			"retrace slow: no command. Use: retrace slow %s --ms %d -- <command>\n",
			func, ms);
		return 1;
	}

	snprintf(json, sizeof(json),
		"{\"intercept_scripts\":[{\"func_name\":\"%s\","
		"\"actions\":[{\"action_name\":\"call_real\"},"
		"{\"action_name\":\"delay\","
		"\"action_params\":{\"ms\":%d}}]}]}",
		func, ms);

	config = write_temp_config(json);
	if (!config)
		return 1;

	launch_target(config, logfile, lib_override, quiet,
		      argc - sep - 1, &argv[sep + 1]);
	return 0;
}

static int cmd_run(int argc, char **argv)
{
	const char *config = NULL;
	const char *logfile = NULL;
	const char *lib_override = NULL;
	int quiet = 0;
	int i;

	/* Parse options before -- */
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0) {
			i++;
			break;
		}
		if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
			config = argv[++i];
		} else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
			logfile = argv[++i];
		} else if (strcmp(argv[i], "--lib") == 0 && i + 1 < argc) {
			lib_override = argv[++i];
		} else if (strcmp(argv[i], "--quiet") == 0) {
			quiet = 1;
		} else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			usage(stdout);
			return 0;
		}
		fprintf(stderr, "retrace: unknown option '%s'\n", argv[i]);
		return 1;
	}

	if (i >= argc) {
		fprintf(stderr, "retrace: no command specified. Use: retrace run [OPTIONS] -- <command>\n");
		return 1;
	}

	launch_target(config, logfile, lib_override, quiet,
		      argc - i, &argv[i]);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		usage(stderr);
		return 1;
	}

	if (strcmp(argv[1], "run") == 0) {
		return cmd_run(argc - 2, &argv[2]);
	}

	if (strcmp(argv[1], "trace") == 0) {
		return cmd_trace(argc - 2, &argv[2]);
	}

	if (strcmp(argv[1], "mock") == 0) {
		return cmd_mock(argc - 2, &argv[2]);
	}

	if (strcmp(argv[1], "fuzz") == 0) {
		return cmd_fuzz(argc - 2, &argv[2]);
	}

	if (strcmp(argv[1], "slow") == 0) {
		return cmd_slow(argc - 2, &argv[2]);
	}

	if (strcmp(argv[1], "pp") == 0) {
		if (argc < 3) {
			fprintf(stderr, "retrace pp: usage: retrace pp <trace.json>\n");
			return 1;
		}
		pp_log(argv[2]);
		return 0;
	}

	if (strcmp(argv[1], "list-functions") == 0) {
		/*
		 * TODO: link against retrace_core and walk the registry.
		 * For now, print a helpful message.
		 */
		printf("retrace list-functions requires the retrace_core library.\n");
		printf("For now, see src/core/prototypes/ for the full list.\n");
		return 0;
	}

	if (strcmp(argv[1], "list-actions") == 0) {
		printf("Built-in actions:\n");
		printf("  log_params              Log call args to JSON\n");
		printf("  call_real               Invoke real libc implementation\n");
		printf("  modify_in_param_str     Rewrite a string argument\n");
		printf("  modify_in_param_int     Rewrite an integer argument\n");
		printf("  modify_in_param_arr     Rewrite a byte array argument\n");
		printf("  modify_return_value_int Override the return value\n");
		printf("  memory_fuzz             Randomly fail malloc/calloc/realloc\n");
		printf("  incomplete_io           Partially fail I/O calls\n");
		printf("  fuzzing_seed            Set deterministic seed for memory_fuzz\n");
		return 0;
	}

	if (strcmp(argv[1], "validate") == 0) {
		if (argc < 3) {
			fprintf(stderr, "retrace validate: missing config file\n");
			return 1;
		}
		/*
		 * TODO: parse JSON and check action/func names.
		 * For now, just check the file exists.
		 */
		if (access(argv[2], R_OK) != 0) {
			fprintf(stderr, "retrace validate: cannot read '%s'\n", argv[2]);
			return 1;
		}
		printf("retrace validate: %s exists (full validation TBD)\n", argv[2]);
		return 0;
	}

	if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
		usage(stdout);
		return 0;
	}

	fprintf(stderr, "retrace: unknown command '%s'\n", argv[1]);
	usage(stderr);
	return 1;
}
