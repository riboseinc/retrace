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
#include <dlfcn.h>

#include "parson.h"
#include "config_builder.h"

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

/* Mirror of retrace_status_t (include/retrace/retrace.h). The CLI
 * stays a standalone binary: it dlsyms library functions but cannot
 * include the full public header without dragging in the build's
 * include paths. Values must stay in sync with the public header.
 */
typedef int retrace_status_t;

static void usage(FILE *out)
{
	fprintf(out,
"retrace v2.29.1 -- userspace libc interceptor\n"
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
"  retrace fuzz-replay <fuzzer-name> <crash-input>\n"
"  retrace attach [OPTIONS] <pid>\n"
"  retrace backends\n"
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
 * HTML trace viewer — built into the CLI, no Python needed.
 * Generates a self-contained interactive HTML page from a JSON log.
 */

static const char *categorize_func(const char *func)
{
	struct {
		const char *kw;
		const char *cat;
	} table[] = {
		{"open", "I/O"}, {"read", "I/O"}, {"write", "I/O"},
		{"close", "I/O"}, {"fopen", "I/O"}, {"stat", "I/O"},
		{"access", "I/O"}, {"lseek", "I/O"},
		{"socket", "NET"}, {"connect", "NET"}, {"bind", "NET"},
		{"listen", "NET"}, {"accept", "NET"}, {"send", "NET"},
		{"recv", "NET"},
		{"malloc", "MEM"}, {"calloc", "MEM"}, {"realloc", "MEM"},
		{"free", "MEM"}, {"memmove", "MEM"}, {"memset", "MEM"},
		{"memcpy", "MEM"}, {"strlen", "MEM"}, {"strcmp", "MEM"},
		{"strcpy", "MEM"},
		{"pthread_mutex", "SYNC"}, {"pthread_cond", "SYNC"},
		{"pthread_rwlock", "SYNC"},
		{"system", "EXEC"}, {"exec", "EXEC"}, {"fork", "EXEC"},
		{"exit", "EXEC"}, {"abort", "EXEC"},
		{"getenv", "ENV"}, {"setenv", "ENV"},
		{"time", "TIME"}, {"clock", "TIME"},
	};
	size_t i;

	for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		if (strstr(func, table[i].kw) != NULL)
			return table[i].cat;
	}
	return "OTHER";
}

static const char *cat_color(const char *cat)
{
	struct {
		const char *cat;
		const char *color;
	} colors[] = {
		{"I/O", "#4a90d9"}, {"NET", "#e8782c"}, {"MEM", "#2ecc71"},
		{"SYNC", "#9b59b6"}, {"EXEC", "#e74c3c"}, {"ENV", "#f1c40f"},
		{"TIME", "#1abc9c"},
	};
	size_t i;

	for (i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
		if (strcmp(cat, colors[i].cat) == 0)
			return colors[i].color;
	}
	return "#95a5a6";
}

static void html_escape(FILE *out, const char *s)
{
	if (s == NULL)
		return;

	for (; *s; s++) {
		switch (*s) {
		case '<':
			fputs("&lt;", out);
			break;
		case '>':
			fputs("&gt;", out);
			break;
		case '&':
			fputs("&amp;", out);
			break;
		case '"':
			fputs("&quot;", out);
			break;
		default:
			fputc(*s, out);
			break;
		}
	}
}

static void html_log(const char *path, FILE *out)
{
	JSON_Value *root;
	JSON_Array *arr;
	JSON_Object *entry, *msg, *pending_args = NULL;
	size_t i, n, j, ci;
	const char *func, *text;
	int count = 0;
	double total_us = 0;
	int cat_counts[8] = {0};
	double cat_times[8] = {0};
	static const char *cat_names[8] = {
		"I/O", "NET", "MEM", "SYNC", "EXEC", "ENV", "TIME", "OTHER"
	};

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

	/* Pass 1: summary stats */
	for (i = 0; i < n; i++) {
		entry = json_array_get_object(arr, i);
		if (entry == NULL)
			continue;
		msg = json_object_get_object(entry, "message");
		if (msg == NULL)
			continue;
		text = json_object_get_string(msg, "text");
		if (text != NULL)
			continue;
		func = json_object_get_string(msg, "func");
		if (func && json_object_has_value(msg, "call_duration_us")) {
			double us = json_object_get_number(msg,
				"call_duration_us");
			const char *cat = categorize_func(func);

			count++;
			total_us += us;
			for (ci = 0; ci < 8; ci++) {
				if (strcmp(cat, cat_names[ci]) == 0) {
					cat_counts[ci]++;
					cat_times[ci] += us;
					break;
				}
			}
		}
	}

	/* HTML prologue + summary */
	fprintf(out,
		"<!DOCTYPE html>\n<html><head><meta charset=\"UTF-8\">\n"
		"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
		"<title>retrace trace</title>\n"
		"<style>\n"
		"* { box-sizing:border-box; margin:0; padding:0; }\n"
		"body { font-family:system-ui,sans-serif; background:#f8f9fa; color:#2c3e50; padding:20px; }\n"
		"h1 { font-size:1.4rem; margin-bottom:4px; }\n"
		".sub { color:#7f8c8d; font-size:.85rem; margin-bottom:16px; }\n"
		".cards { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:16px; }\n"
		".card { background:#fff; border-radius:8px; padding:10px 14px; box-shadow:0 1px 3px rgba(0,0,0,.1); }\n"
		".card .l { font-size:.65rem; text-transform:uppercase; color:#95a5a6; }\n"
		".card .v { font-size:1.3rem; font-weight:700; }\n"
		".cats { margin-bottom:16px; }\n"
		".cat { font-size:.8rem; margin-bottom:3px; }\n"
		".badge { display:inline-block; padding:1px 7px; border-radius:3px; color:#fff; font-size:.65rem; font-weight:700; min-width:28px; text-align:center; }\n"
		"input { padding:7px 10px; border:1px solid #ddd; border-radius:4px; font-size:.8rem; width:100%%; margin-bottom:10px; }\n"
		"table { width:100%%; border-collapse:collapse; background:#fff; border-radius:8px; overflow:hidden; box-shadow:0 1px 3px rgba(0,0,0,.1); }\n"
		"th { background:#2c3e50; color:#fff; padding:7px 10px; text-align:left; font-size:.7rem; text-transform:uppercase; }\n"
		"td { padding:5px 10px; border-bottom:1px solid #ecf0f1; font-size:.8rem; }\n"
		"tr:hover { background:#f8f9fa; }\n"
		".fn { font-family:monospace; font-weight:600; }\n"
		".ar { font-family:monospace; font-size:.72rem; color:#7f8c8d; }\n"
		".du { font-family:monospace; text-align:right; font-weight:600; }\n"
		"</style></head><body>\n"
		"<h1>retrace trace</h1>\n"
		"<p class=\"sub\">%d calls intercepted &middot; %.1fms total libc time</p>\n"
		"<div class=\"cards\">"
		"<div class=\"card\"><div class=\"l\">Calls</div><div class=\"v\">%d</div></div>"
		"<div class=\"card\"><div class=\"l\">Total</div><div class=\"v\">%.1fms</div></div>"
		"<div class=\"card\"><div class=\"l\">Avg</div><div class=\"v\">%.0f&micro;s</div></div>"
		"</div>\n<div class=\"cats\">\n",
		count, total_us / 1000.0,
		count, total_us / 1000.0,
		count > 0 ? total_us / count : 0.0);

	/* Category breakdown */
	for (ci = 0; ci < 8; ci++) {
		if (cat_counts[ci] > 0) {
			fprintf(out,
				"<div class=\"cat\"><span class=\"badge\" style=\"background:%s\">%s</span>"
				" %d calls &middot; %.1fms</div>\n",
				cat_color(cat_names[ci]), cat_names[ci],
				cat_counts[ci], cat_times[ci] / 1000.0);
		}
	}
	fprintf(out, "</div>\n");

	/* Filter + table */
	fprintf(out,
		"<input type=\"text\" placeholder=\"Filter by function name...\" "
		"id=\"f\" onkeyup=\"var q=this.value.toLowerCase();"
		"document.querySelectorAll('.r').forEach(function(r){"
		"r.style.display=r.textContent.toLowerCase().includes(q)?'':'none';})\">\n"
		"<table><thead><tr><th>Cat</th><th>Function</th>"
		"<th>Args</th><th>Return</th><th>Duration</th></tr></thead><tbody>\n");

	/* Pass 2: data rows */
	for (i = 0; i < n; i++) {
		entry = json_array_get_object(arr, i);
		if (entry == NULL)
			continue;
		msg = json_object_get_object(entry, "message");
		if (msg == NULL)
			continue;
		text = json_object_get_string(msg, "text");
		if (text != NULL)
			continue;
		func = json_object_get_string(msg, "func");
		if (func && json_object_has_value(msg, "call_duration_us")) {
			double us = json_object_get_number(msg,
				"call_duration_us");
			double rv = json_object_get_number(msg, "ret_val");
			const char *cat = categorize_func(func);

			fprintf(out,
				"<tr class=\"r\" style=\"border-left:3px solid %s\">"
				"<td><span class=\"badge\" style=\"background:%s\">%s</span></td>"
				"<td class=\"fn\">", cat_color(cat),
				cat_color(cat), cat);
			html_escape(out, func);
			fprintf(out, "</td><td class=\"ar\">");
			if (pending_args != NULL) {
				size_t nk = json_object_get_count(pending_args);

				for (j = 0; j < nk; j++) {
					const char *k = json_object_get_name(
						pending_args, j);
					const char *v = json_value_get_string(
						json_object_get_value_at(
							pending_args, j));

					fprintf(out, "%s=", k);
					html_escape(out, v);
					fprintf(out, " ");
				}
				pending_args = NULL;
			}
			fprintf(out, "</td><td>%g</td>", rv);
			if (us < 1.0)
				fprintf(out,
					"<td class=\"du\">&lt;1&micro;s</td>");
			else
				fprintf(out,
					"<td class=\"du\">%.0f&micro;s</td>",
					us);
			fprintf(out, "</tr>\n");
		} else if (func == NULL) {
			pending_args = msg;
		}
	}

	fprintf(out, "</tbody></table>\n</body></html>\n");
	json_value_free(root);
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
	int html_mode = 0;
	int sep, i, nfuncs = 0;
	char json[8192];
	const char *config;

	/* Count func names until we hit an option or -- */
	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-' || strcmp(argv[i], "--") == 0)
			break;
		nfuncs++;
	}

	/* Check for --html before parse_common_opts */
	for (sep = 0; sep < argc; sep++) {
		if (strcmp(argv[sep], "--html") == 0) {
			html_mode = 1;
			break;
		}
		if (strcmp(argv[sep], "--") == 0)
			break;
	}

	i = parse_common_opts(argc, argv, i, &logfile, &quiet, &lib_override);
	sep = find_separator(argc, argv);
	if (sep < 0 || sep + 1 >= argc) {
		fprintf(stderr,
			"retrace trace: no command. Use: retrace trace [funcs...] -- <command>\n");
		return 1;
	}

	if (nfuncs == 0) {
		if (retrace_cli_build_trace_config(json, sizeof(json),
						   NULL, 0) < 0) {
			fprintf(stderr,
				"retrace trace: config too large for buffer\n");
			return 1;
		}
	} else {
		if (retrace_cli_build_trace_config(json, sizeof(json),
						   (const char *const *)argv,
						   (size_t)nfuncs) < 0) {
			fprintf(stderr,
				"retrace trace: config too large for buffer\n");
			return 1;
		}
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

		/* Pretty-print or generate HTML from the trace log */
		if (html_mode) {
			char html_path[PATH_MAX];
			FILE *hf;

			snprintf(html_path, sizeof(html_path),
				 "/tmp/retrace-%d.html", (int)getpid());
			hf = fopen(html_path, "w");
			if (hf) {
				html_log(trace_log, hf);
				fclose(hf);
				fprintf(stderr, "wrote %s\n", html_path);
			} else {
				perror("retrace: html");
				html_log(trace_log, stdout);
			}
		} else {
			pp_log(trace_log);
		}

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

	if (retrace_cli_build_mock_config(json, sizeof(json),
					  func, retval) < 0) {
		fprintf(stderr,
			"retrace mock: config too large for buffer\n");
		return 1;
	}

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

	if (retrace_cli_build_fuzz_config(json, sizeof(json),
					  func, rate) < 0) {
		fprintf(stderr,
			"retrace fuzz: config too large for buffer\n");
		return 1;
	}

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

	if (retrace_cli_build_slow_config(json, sizeof(json),
					  func, ms) < 0) {
		fprintf(stderr,
			"retrace slow: config too large for buffer\n");
		return 1;
	}

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

/*
 * retrace attach [OPTIONS] <pid>
 *
 * Attach to an already-running process and trace its syscalls until
 * it exits. Uses the ptrace backend — no LD_PRELOAD, no restart.
 * The trace output follows the same JSON format as `retrace run`
 * and feeds the same downstream tools.
 */
static int cmd_attach(int argc, char **argv)
{
	const char *config = NULL;
	const char *logfile = NULL;
	const char *lib_override = NULL;
	int quiet = 0;
	int pid = -1;
	int i;
	char lib_path[PATH_MAX];
	char *found;
	void *lib;
	retrace_status_t (*attach_fn)(int pid);
	retrace_status_t rc;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
			config = argv[++i];
		} else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
			logfile = argv[++i];
		} else if (strcmp(argv[i], "--lib") == 0 && i + 1 < argc) {
			lib_override = argv[++i];
		} else if (strcmp(argv[i], "--quiet") == 0) {
			quiet = 1;
		} else if (strcmp(argv[i], "--help") == 0 ||
			   strcmp(argv[i], "-h") == 0) {
			usage(stdout);
			return 0;
		} else if (pid < 0) {
			char *end = NULL;
			long v = strtol(argv[i], &end, 10);

			if (end == argv[i] || *end != '\0' || v <= 0) {
				fprintf(stderr,
					"retrace attach: invalid pid '%s'\n",
					argv[i]);
				return 1;
			}
			pid = (int)v;
		} else {
			fprintf(stderr, "retrace attach: unknown option '%s'\n",
				argv[i]);
			return 1;
		}
	}

	if (pid < 0) {
		fprintf(stderr,
			"retrace attach: usage: retrace attach [--config FILE] "
			"[--log FILE] <pid>\n");
		return 1;
	}

	if (lib_override)
		setenv("RETRACE_LIB", lib_override, 1);

	found = find_library(lib_path, sizeof(lib_path));
	if (!found) {
		fprintf(stderr,
			"retrace attach: cannot find %s. Set RETRACE_LIB.\n",
			RETRACE_LIB_NAME);
		return 1;
	}

	/* The library constructor reads these at dlopen time. */
	if (config)
		setenv("RETRACE_JSON_CONFIG", config, 1);
	if (logfile) {
		setenv("RETRACE_LOGGER_DEF_FN", logfile, 1);
		setenv("RETRACE_LOGGER_DEF_STDOUT_ENA", "0", 1);
	}
	if (quiet)
		setenv("RETRACE_LOGGER_DEF_ENA", "0", 1);

	lib = dlopen(lib_path, RTLD_NOW);
	if (lib == NULL) {
		fprintf(stderr, "retrace attach: dlopen(%s): %s\n",
			lib_path, dlerror());
		return 1;
	}

	attach_fn = (retrace_status_t (*)(int pid))dlsym(lib,
		"retrace_attach_process");
	if (attach_fn == NULL) {
		fprintf(stderr,
			"retrace attach: library lacks retrace_attach_process "
			"(pre-v2.4.0 build?)\n");
		dlclose(lib);
		return 1;
	}

	fprintf(stderr, "retrace attach: tracing pid %d (ctrl-c to stop "
		"tracing, target exits when it exits)\n", pid);

	rc = attach_fn(pid);

	switch (rc) {
	case 0:
		fprintf(stderr, "retrace attach: target exited\n");
		break;
	case -7: /* RETRACE_ERR_PERMISSION */
		fprintf(stderr,
			"retrace attach: permission denied (ptrace).\n"
			"  - run as root, or\n"
			"  - attach to a child process, or\n"
			"  - set /proc/sys/kernel/yama/ptrace_scope to 0 "
			"(security tradeoff)\n");
		break;
	case -6: /* RETRACE_ERR_UNSUPPORTED */
		fprintf(stderr,
			"retrace attach: unsupported on this platform "
			"(Linux only)\n");
		break;
	case -3: /* RETRACE_ERR_NOENT */
		fprintf(stderr,
			"retrace attach: ptrace backend not available in "
			"this build\n");
		break;
	default:
		fprintf(stderr, "retrace attach: failed (status %d)\n",
			(int)rc);
		break;
	}

	dlclose(lib);
	return rc == 0 ? 0 : 1;
}

/*
 * retrace backends
 *
 * List the backends compiled into this build (preload_elf, preload_macho,
 * ptrace, ...). Discoverability for the interposition mechanisms
 * available on this platform.
 */
static int cmd_backends(void)
{
	char lib_path[PATH_MAX];
	char *found;
	void *lib;
	retrace_status_t (*list_fn)(const char *const **names,
				    size_t *count);
	const char *const *names;
	size_t count;
	size_t i;

	/* Pure listing: keep the library's constructor log out of the
	 * way (it would otherwise emit a JSON banner to stdout).
	 */
	setenv("RETRACE_LOGGER_DEF_STDOUT_ENA", "0", 1);

	found = find_library(lib_path, sizeof(lib_path));
	if (!found) {
		fprintf(stderr,
			"retrace backends: cannot find %s. Set RETRACE_LIB.\n",
			RETRACE_LIB_NAME);
		return 1;
	}

	lib = dlopen(lib_path, RTLD_NOW);
	if (lib == NULL) {
		fprintf(stderr, "retrace backends: dlopen(%s): %s\n",
			lib_path, dlerror());
		return 1;
	}

	list_fn = (retrace_status_t(*)(const char *const **names,
				       size_t *count))
		dlsym(lib, "retrace_list_backends");
	if (list_fn == NULL) {
		fprintf(stderr,
			"retrace backends: library lacks "
			"retrace_list_backends (pre-v2.4.0 build?)\n");
		dlclose(lib);
		return 1;
	}

	if (list_fn(&names, &count) != 0 || count == 0) {
		fprintf(stderr, "retrace backends: none registered\n");
		dlclose(lib);
		return 1;
	}

	printf("%zu backend%s registered:\n", count, count == 1 ? "" : "s");
	for (i = 0; i < count; i++)
		printf("  %s\n", names[i]);

	dlclose(lib);
	return 0;
}

/*
 * retrace list-functions / list-actions
 *
 * Prints the registry the library actually carries (dlopen +
 * dlsym of the public introspection API -- the names come from
 * whatever library this build installed, not a hardcoded list).
 */
static int cmd_list_registry(const char *api_symbol)
{
	char lib_path[PATH_MAX];
	char *found;
	void *lib;
	retrace_status_t (*list_fn)(const char *const **names,
				    size_t *count);
	const char *const *names = NULL;
	size_t count = 0;
	size_t i;

	setenv("RETRACE_LOGGER_DEF_STDOUT_ENA", "0", 1);
	found = find_library(lib_path, sizeof(lib_path));
	if (!found) {
		fprintf(stderr,
			"retrace: cannot find %s. Set RETRACE_LIB.\n",
			RETRACE_LIB_NAME);
		return 1;
	}

	lib = dlopen(lib_path, RTLD_NOW);
	if (lib == NULL) {
		fprintf(stderr, "retrace: dlopen(%s): %s\n",
			lib_path, dlerror());
		return 1;
	}

	list_fn = (retrace_status_t(*)(const char *const **names,
				       size_t *count))dlsym(lib, api_symbol);
	if (list_fn == NULL) {
		fprintf(stderr,
			"retrace: library lacks %s (pre-v2.6.0 build?)\n",
			api_symbol);
		dlclose(lib);
		return 1;
	}

	if (list_fn(&names, &count) != 0 || count == 0) {
		fprintf(stderr, "retrace: registry empty\n");
		dlclose(lib);
		return 1;
	}

	printf("%zu entr%s:\n", count, count == 1 ? "y" : "ies");
	for (i = 0; i < count; i++)
		printf("  %s\n", names[i]);

	dlclose(lib);
	return 0;
}

/*
 * retrace validate <config.json>
 *
 * Parses the config (comment-tolerant) and checks every
 * func_name/action_name against the library's registries via
 * the public retrace_config_validate_buffer -- real error
 * messages, not just file-existence.
 */
static int cmd_validate(int argc, char **argv)
{
	char lib_path[PATH_MAX];
	char *found;
	void *lib;
	retrace_status_t (*validate_fn)(const char *buf, size_t len,
					char *err_buf, size_t err_len);
	FILE *f;
	long fz;
	char *buf;
	char err[256];
	int rc;

	if (argc < 3) {
		fprintf(stderr, "retrace validate: missing config file\n");
		return 2;
	}
	f = fopen(argv[2], "r");
	if (f == NULL) {
		fprintf(stderr, "retrace validate: cannot read '%s'\n",
			argv[2]);
		return 2;
	}
	fseek(f, 0, SEEK_END);
	fz = ftell(f);
	rewind(f);
	if (fz < 0) {
		fclose(f);
		return 2;
	}
	buf = (char *)malloc((size_t)fz + 1);
	if (buf == NULL || fread(buf, 1, (size_t)fz, f) != (size_t)fz) {
		fprintf(stderr, "retrace validate: read failed\n");
		free(buf);
		fclose(f);
		return 2;
	}
	buf[fz] = '\0';
	fclose(f);

	setenv("RETRACE_LOGGER_DEF_STDOUT_ENA", "0", 1);
	found = find_library(lib_path, sizeof(lib_path));
	if (!found) {
		fprintf(stderr,
			"retrace validate: cannot find %s. Set RETRACE_LIB.\n",
			RETRACE_LIB_NAME);
		free(buf);
		return 2;
	}
	lib = dlopen(lib_path, RTLD_NOW);
	if (lib == NULL) {
		fprintf(stderr, "retrace validate: dlopen(%s): %s\n",
			lib_path, dlerror());
		free(buf);
		return 2;
	}
	validate_fn = (retrace_status_t (*)(const char *, size_t,
					    char *, size_t))dlsym(lib,
		"retrace_config_validate_buffer");
	if (validate_fn == NULL) {
		fprintf(stderr,
			"retrace validate: library lacks "
			"retrace_config_validate_buffer "
			"(pre-v2.6.1 build?)\n");
		dlclose(lib);
		free(buf);
		return 2;
	}

	rc = validate_fn(buf, (size_t)fz, err, sizeof(err));
	free(buf);
	dlclose(lib);

	if (rc == 0) {
		printf("ok: config is valid\n");
		return 0;
	}
	if (rc == (retrace_status_t)-2) {
		fprintf(stderr, "retrace validate: invalid input\n");
		return 2;
	}
	fprintf(stderr, "retrace validate: %s\n",
		err[0] ? err : "invalid config");
	return 1;
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

	if (strcmp(argv[1], "attach") == 0) {
		return cmd_attach(argc - 2, &argv[2]);
	}

	if (strcmp(argv[1], "backends") == 0) {
		return cmd_backends();
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

	if (strcmp(argv[1], "html") == 0) {
		if (argc < 3) {
			fprintf(stderr, "retrace html: usage: retrace html <trace.json> [-o output.html]\n");
			return 1;
		}
		if (argc >= 5 && strcmp(argv[3], "-o") == 0) {
			FILE *f = fopen(argv[4], "w");

			if (!f) {
				perror("retrace html");
				return 1;
			}
			html_log(argv[2], f);
			fclose(f);
			fprintf(stderr, "wrote %s\n", argv[4]);
		} else {
			html_log(argv[2], stdout);
		}
		return 0;
	}

	if (strcmp(argv[1], "list-functions") == 0)
		return cmd_list_registry("retrace_list_functions");

	if (strcmp(argv[1], "list-actions") == 0)
		return cmd_list_registry("retrace_list_actions");

	if (strcmp(argv[1], "validate") == 0) {
		return cmd_validate(argc, argv);
	}

	if (strcmp(argv[1], "fuzz-replay") == 0) {
		/*
		 * retrace fuzz-replay <fuzzer-name> <crash-input>
		 *
		 * Replays a crashing input through the named libFuzzer
		 * harness. The harness runs the input once (not fuzzing),
		 * and if it crashes, the backtrace is visible in stderr.
		 *
		 * Pairs with the nightly fuzz workflow (TODO.complete/33):
		 * download the crash artifact, then run:
		 *
		 *   retrace fuzz-replay fuzz_action_run crash-abc123
		 */
		const char *fuzzer_name;
		const char *crash_path;
		char fuzzer_bin[PATH_MAX];

		if (argc < 4) {
			fprintf(stderr,
				"retrace fuzz-replay: usage: retrace fuzz-replay <fuzzer-name> <crash-input>\n"
				"  <fuzzer-name>   e.g. fuzz_config_parse, fuzz_action_run\n"
				"  <crash-input>   path to the crash reproducer file\n");
			return 1;
		}

		fuzzer_name = argv[2];
		crash_path = argv[3];

		if (access(crash_path, R_OK) != 0) {
			fprintf(stderr,
				"retrace fuzz-replay: cannot read '%s'\n",
				crash_path);
			return 1;
		}

		/* Try to find the fuzzer binary:
		 *   1. As-is via PATH (execvp handles lookup)
		 *   2. Alongside the CLI binary
		 */
		snprintf(fuzzer_bin, sizeof(fuzzer_bin), "%s", fuzzer_name);
		if (strchr(fuzzer_name, '/') == NULL) {
			/* No slash in name -- might be on PATH.
			 * execvp will search PATH; skip access() check.
			 */
		} else if (access(fuzzer_bin, X_OK) != 0) {
			fprintf(stderr,
				"retrace fuzz-replay: cannot execute '%s'\n",
				fuzzer_name);
			return 1;
		}

		fprintf(stderr,
			"retrace fuzz-replay: running %s on %s ...\n",
			fuzzer_name, crash_path);

		/* libFuzzer in single-input mode: just pass the file
		 * path. The harness runs LLVMFuzzerTestOneInput once
		 * and exits. If the input triggers a crash, the
		 * backtrace goes to stderr (ASAN).
		 */
		execlp(fuzzer_bin, fuzzer_name, crash_path, (char *)NULL);

		perror("retrace fuzz-replay: exec failed");
		return 127;
	}

	if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
		usage(stdout);
		return 0;
	}

	fprintf(stderr, "retrace: unknown command '%s'\n", argv[1]);
	usage(stderr);
	return 1;
}
