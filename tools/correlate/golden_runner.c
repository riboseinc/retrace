/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Golden-fixture runner (TODO.next-level/02). Runs the
 * retrace-correlate CLI over one case directory and asserts the
 * parity contract from tools/correlate/golden/README.md: exact
 * stdout and exit code.
 *
 * usage: correlate-golden <case_dir> <tool_path> <capture_path>
 *
 * Portable spawn: system() with double-quoted paths and shell
 * redirection ("> file" behaves identically in POSIX sh and
 * cmd.exe for our fixed argv shape).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#define RUNNER_MAX 8192

static char *
read_all(const char *path, size_t *len_out)
{
	FILE *f = fopen(path, "rb");
	long sz;
	char *buf = NULL;

	if (f == NULL)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0)
		goto fail;
	sz = ftell(f);
	if (sz < 0)
		goto fail;
	if (fseek(f, 0, SEEK_SET) != 0)
		goto fail;
	buf = (char *)malloc((size_t)sz + 1);
	if (buf == NULL)
		goto fail;
	if (fread(buf, 1, (size_t)sz, f) != (size_t)sz)
		goto fail;
	fclose(f);
	buf[sz] = '\0';
	*len_out = (size_t)sz;
	return buf;

fail:
	fclose(f);
	free(buf);
	return NULL;
}

/* Strip one trailing newline (LF or CRLF) from a file's content. */
static void
chomp(char *s)
{
	size_t n = strlen(s);

	while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
		s[--n] = '\0';
}

static char *
read_line_file(const char *path)
{
	size_t n = 0;
	char *s = read_all(path, &n);

	if (s == NULL)
		return NULL;
	chomp(s);
	return s;
}

int
main(int argc, char **argv)
{
	char   path[RUNNER_MAX];
	char   cmd[RUNNER_MAX * 3];
	char *prefix, *options = NULL, *expected, *want_exit_s,
	      *actual;
	size_t expected_len = 0, actual_len = 0;
	int    rc, got_exit, want_exit;

	if (argc != 4) {
		fprintf(stderr,
			"Usage: correlate-golden <case_dir> "
			"<tool_path> <capture_path>\n");
		return 2;
	}

	snprintf(path, sizeof(path), "%s/prefix.txt", argv[1]);
	prefix = read_line_file(path);
	snprintf(path, sizeof(path), "%s/expected.txt", argv[1]);
	expected = read_all(path, &expected_len);
	snprintf(path, sizeof(path), "%s/exit.txt", argv[1]);
	want_exit_s = read_line_file(path);
	if (prefix == NULL || expected == NULL || want_exit_s == NULL) {
		fprintf(stderr,
			"golden: case %s is missing fixture "
			"files\n",
			argv[1]);
		return 2;
	}
	want_exit = atoi(want_exit_s);

	/*
	 * Optional per-case extra flags (options.txt), verbatim:
	 * --pid N, --window S, --exclude-probes.
	 */
	snprintf(path, sizeof(path), "%s/options.txt", argv[1]);
	options = read_line_file(path);

	if (options != NULL && options[0] != '\0')
		snprintf(cmd,
			 sizeof(cmd),
			 "\"%s\" --inside \"%s/inside.json\" "
			 "--outside \"%s/outside.json\" "
			 "--prefix \"%s\" %s > \"%s\"",
			 argv[2],
			 argv[1],
			 argv[1],
			 prefix,
			 options,
			 argv[3]);
	else
		snprintf(cmd,
			 sizeof(cmd),
			 "\"%s\" --inside \"%s/inside.json\" "
			 "--outside \"%s/outside.json\" "
			 "--prefix \"%s\" > \"%s\"",
			 argv[2],
			 argv[1],
			 argv[1],
			 prefix,
			 argv[3]);
	rc = system(cmd);
#ifdef _WIN32
	got_exit = rc;
#else
	got_exit = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
#endif

	actual = read_all(argv[3], &actual_len);
	if (actual == NULL) {
		fprintf(stderr, "golden: %s produced no capture\n", argv[1]);
		return 1;
	}
	if (actual_len != expected_len || memcmp(actual, expected, expected_len) != 0) {
		fprintf(stderr,
			"golden: %s stdout mismatch\n"
			"--- expected ---\n%s\n--- actual ---\n%s\n",
			argv[1],
			expected,
			actual);
		return 1;
	}
	if (got_exit != want_exit) {
		fprintf(stderr, "golden: %s exit %d, want %d\n", argv[1], got_exit, want_exit);
		return 1;
	}

	printf("  golden %-20s OK\n", argv[1]);
	return 0;
}
