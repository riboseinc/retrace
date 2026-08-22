/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-fuzz-report -- the fuzzing workbench driver
 * (TODO.trace-profile/20). Runs a corpus of seeds against a
 * target under a fuzz config, classifies each iteration
 * (crash / assertion / clean), clusters failures by
 * (last-called function, param count), and emits:
 *
 *   report.json            -- the summary (shape: docs/reports.md)
 *   repro-<cluster>.json   -- one reproducer per failure cluster:
 *                             the SAME config + the RETRACE_FUZZ_SEED
 *                             env value that produced the failure
 *
 * usage:
 *   retrace-fuzz-report --config fuzz.json --seeds <dir> \
 *     [--lib <libretrace>] [--marker ASSERT] [-o report.json] \
 *     [-- cmd args...]
 *
 * Seeds are files in --seeds; the RETRACE_FUZZ_SEED for an
 * iteration is the FNV-1a of the seed file's CONTENT (same
 * seed file -> same fuzz sequence). memory_fuzz reads the env
 * seed when its config has no explicit fuzz_seed (>= 2.21.0).
 * Exit 1 when any crash cluster exists (CI-able).
 *
 * POSIX only in v1 (fork/exec); Windows honestly refused.
 */

#include "cluster.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32

#include <dirent.h>
#include <sys/wait.h>
#include <unistd.h>

#include "parson.h"

static void usage(FILE *out)
{
	fprintf(out,
"Usage: retrace-fuzz-report --config <fuzz.json> --seeds <dir>\n"
"       [--lib <libretrace>] [--marker <substr>] [-o report.json]\n"
"       [-- cmd args...]\n"
"\n"
"Runs the target once per seed under the fuzz config; clusters\n"
"failures; emits report.json + per-cluster repro configs.\n");
}

static unsigned long fnv1a_buf(const char *s, size_t n,
			       unsigned long h)
{
	size_t i;

	for (i = 0; i < n; i++) {
		h ^= (unsigned char)s[i];
		h *= 0x01000193UL;
	}
	return h;
}

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static const char *find_lib(const char *arg)
{
	static char buf[1024];
	const char *env = getenv("RETRACE_V2_LIB");
	const char *const exts[] = { "so", "dylib" };
	size_t e;

	if (arg != NULL)
		return arg;
	if (env != NULL && env[0] != '\0')
		return env;
	for (e = 0; e < 2; e++) {
		snprintf(buf, sizeof(buf), "src/v2/libretrace.%s",
			exts[e]);
		if (access(buf, R_OK) == 0)
			return buf;
	}
	return NULL;
}

/* run one iteration; returns waitpid status; trace read back */
static int run_iter(char *const argv[], const char *lib,
	unsigned long seed, const char *trace_path)
{
	pid_t pid = fork();
	int status = 0;
	char seed_env[32];

	if (pid < 0)
		return -1;
	if (pid == 0) {
		char logenv[1024];

		snprintf(seed_env, sizeof(seed_env), "%lu", seed);
		setenv("RETRACE_FUZZ_SEED", seed_env, 1);
		snprintf(logenv, sizeof(logenv), "%s",
			trace_path);
		setenv("RETRACE_LOGGER_DEF_FN", logenv, 1);
		setenv("RETRACE_LOGGER_DEF_ENA", "1", 1);
		setenv("RETRACE_LOGGER_DEF_STDOUT_ENA", "0", 1);
#ifdef __APPLE__
		setenv("DYLD_INSERT_LIBRARIES", lib, 1);
#else
		setenv("LD_PRELOAD", lib, 1);
#endif
		execvp(argv[0], argv);
		_exit(127);
	}
	if (waitpid(pid, &status, 0) < 0)
		return -1;
	return status;
}

static char *read_file(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	long sz;
	char *buf;

	if (f == NULL)
		return NULL;
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0) {
		fclose(f);
		return NULL;
	}
	buf = (char *)malloc((size_t)sz + 1);
	if (buf == NULL || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		fclose(f);
		return NULL;
	}
	fclose(f);
	buf[sz] = '\0';
	if (len != NULL)
		*len = (size_t)sz;
	return buf;
}

int main(int argc, char **argv)
{
	const char *config = NULL;
	const char *seeds_dir = NULL;
	const char *lib_arg = NULL;
	const char *out_path = "fuzz-report.json";
	const char *marker = NULL;
	char *const *cmd = NULL;
	int cmd_start = -1;
	int i;
	const char *lib;
	char work[256];
	struct FuzzReport rep;
	JSON_Value *jv;
	char *ser;
	int exit_code = 0;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
			config = argv[++i];
		else if (strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
			seeds_dir = argv[++i];
		else if (strcmp(argv[i], "--lib") == 0 && i + 1 < argc)
			lib_arg = argv[++i];
		else if (strcmp(argv[i], "--marker") == 0 && i + 1 < argc)
			marker = argv[++i];
		else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
			out_path = argv[++i];
		else if (strcmp(argv[i], "--") == 0) {
			cmd_start = i + 1;
			break;
		}
		usage(stderr);
		return 2;
	}
	if (config == NULL || seeds_dir == NULL || cmd_start < 0 ||
	    argv[cmd_start] == NULL) {
		usage(stderr);
		return 2;
	}
	cmd = &argv[cmd_start];

	lib = find_lib(lib_arg);
	if (lib == NULL) {
		fprintf(stderr,
		"retrace-fuzz-report: libretrace not found (--lib / RETRACE_V2_LIB)\n");
		return 2;
	}
	setenv("RETRACE_JSON_CONFIG", config, 1);

	{
		DIR *d = opendir(seeds_dir);
		struct dirent *de;
		char **names = NULL;
		size_t nn = 0, cap = 0, k;

		if (d == NULL) {
			perror(seeds_dir);
			return 2;
		}
		while ((de = readdir(d)) != NULL) {
			if (de->d_name[0] == '.')
				continue;
			if (nn == cap) {
				cap = cap == 0 ? 16 : cap * 2;
				names = (char **)realloc(names,
					cap * sizeof(*names));
				if (names == NULL)
					return 2;
			}
			names[nn] = strdup(de->d_name);
			if (names[nn] == NULL)
				return 2;
			nn++;
		}
		closedir(d);
		qsort(names, nn, sizeof(*names), cmp_str);

		fuzz_report_init(&rep);
		snprintf(work, sizeof(work), "/tmp/retrace-fuzz-%d",
			(int)getpid());

		for (k = 0; k < nn; k++) {
			char seed_path[1024];
			char trace_path[1100];
			char *content;
			size_t clen = 0;
			unsigned long seed;
			int status;
			char *trace = NULL;
			unsigned long cid;

			snprintf(seed_path, sizeof(seed_path), "%s/%s",
				seeds_dir, names[k]);
			snprintf(trace_path, sizeof(trace_path),
				"%s-trace-%zu.json", work, k);
			content = read_file(seed_path, &clen);
			if (content == NULL)
				continue;
			seed = fnv1a_buf(content, clen, 0x811C9DC5UL);
			free(content);

			status = run_iter(cmd, lib, seed, trace_path);
			trace = read_file(trace_path, NULL);
			cid = fuzz_report_fold(&rep, status, trace, seed,
				marker);
			if (cid != 0) {
				printf("  seed %s -> cluster %lu (%s)\n",
					names[k], cid,
					WIFSIGNALED(status) ? "crash" :
					"assertion");
			}
			free(trace);
			remove(trace_path);
		}

		/* repro configs: config + seed env per cluster */
		jv = fuzz_report_to_json(&rep);
		{
			JSON_Array *arr = json_object_get_array(
				json_value_get_object(jv), "clusters");
			size_t ci;

			for (ci = 0; ci < json_array_get_count(arr);
				ci++) {
				JSON_Object *c = json_array_get_object(arr,
					ci);
				const char *id_s = json_object_get_string(
					c, "id");
				const char *seed_s = json_object_get_string(
					c, "seed");
				char repro_path[512];
				FILE *rf;

				snprintf(repro_path,
					sizeof(repro_path),
					"fuzz-repro-%s.json", id_s);
				rf = fopen(repro_path, "w");
				if (rf == NULL)
					continue;
				fprintf(rf,
"{\n  \"reproduce\": \"RETRACE_FUZZ_SEED=%s ",
					seed_s);
				fprintf(rf,
"LD_PRELOAD=%s RETRACE_JSON_CONFIG=%s <cmd>\\n\",\n  \"cluster\": \"%s\",\n  \"func\": \"%s\"\n}\n",
					lib, config, id_s,
					json_object_get_string(c, "func"));
				fclose(rf);
				json_object_set_string(c, "repro",
					repro_path);
			}
		}

		ser = json_serialize_to_string_pretty(jv);
		{
			FILE *of = fopen(out_path, "w");

			if (of == NULL) {
				perror(out_path);
				return 2;
			}
			fprintf(of, "%s\n", ser);
			fclose(of);
		}
		json_free_serialized_string(ser);
		json_value_free(jv);

		fprintf(stderr,
"retrace-fuzz-report: %zu iterations, %zu crashes, %zu assertions, %zu clusters -> %s\n",
			rep.total, rep.crashes, rep.assertions,
			rep.count, out_path);
		if (rep.crashes > 0)
			exit_code = 1;
		fuzz_report_free(&rep);
	}
	return exit_code;
}

#else /* _WIN32 */

int main(void)
{
	fprintf(stderr,
"retrace-fuzz-report: POSIX only in v1 (fork/exec)\n");
	return 2;
}

#endif
