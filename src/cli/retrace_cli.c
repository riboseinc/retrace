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
"  retrace list-functions\n"
"  retrace list-actions\n"
"  retrace validate <config.json>\n"
"\n"
"Options (run):\n"
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

static int cmd_run(int argc, char **argv)
{
	const char *config = NULL;
	const char *logfile = NULL;
	const char *lib_override = NULL;
	int quiet = 0;
	int i;
	char lib_path[PATH_MAX];
	char *found;

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

	/* Find the library */
	if (lib_override) {
		setenv("RETRACE_LIB", lib_override, 1);
	}
	found = find_library(lib_path, sizeof(lib_path));
	if (!found) {
		fprintf(stderr,
			"retrace: cannot find %s. Set RETRACE_LIB or install it.\n",
			RETRACE_LIB_NAME);
		return 1;
	}

	/* Set up environment */
	setenv(RETRACE_PRELOAD_ENV, lib_path, 1);

	if (config)
		setenv("RETRACE_JSON_CONFIG", config, 1);

	if (logfile) {
		setenv("RETRACE_LOGGER_DEF_FN", logfile, 1);
		setenv("RETRACE_LOGGER_DEF_STDOUT_ENA", "0", 1);
	}

	if (quiet) {
		setenv("RETRACE_LOGGER_DEF_ENA", "0", 1);
	}

	/* Exec the target command */
	execvp(argv[i], &argv[i]);

	/* If we get here, exec failed */
	perror("retrace: exec failed");
	return 127;
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
