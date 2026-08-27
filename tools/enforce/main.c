/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-enforce (TODO.beyond-libc/01): the generic kernel-
 * enforcement installer. Reads the spec emitted by
 * `retrace-profile enforce`, applies its Landlock ruleset and
 * seccomp floor, and execs the command -- the kernel-filter
 * deployment in one line:
 *
 *   retrace-enforce spec.json -- ./target args...
 *
 * Fail-closed: if a requested plane cannot be installed, the
 * exec never happens (exit 2) -- unless --allow-missing is
 * given for dev workflows.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "enforce_spec.h"
#include "landlock_apply.h"
#include "seccomp_apply.h"

static void enforce_usage(void)
{
	fprintf(stderr,
		"usage: retrace-enforce [--allow-missing] spec.json"
		" -- cmd [args...]\n"
		"  installs the spec's kernel filters, then execs cmd\n"
		"  --allow-missing: proceed when the kernel lacks a"
		" plane (dev only)\n");
}

int main(int argc, char **argv)
{
	struct enforce_spec spec;
	const char *spec_path = NULL;
	int allow_missing = 0;
	int i;
	int rc;
	FILE *f;
	char *json;
	long sz;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--allow-missing") == 0) {
			allow_missing = 1;
		} else if (strcmp(argv[i], "--") == 0) {
			break;
		} else if (argv[i][0] != '-' && spec_path == NULL) {
			spec_path = argv[i];
		} else {
			enforce_usage();
			return 2;
		}
	}
	if (spec_path == NULL || i + 1 >= argc) {
		enforce_usage();
		return 2;
	}
	f = fopen(spec_path, "rb");
	if (f == NULL) {
		perror("spec");
		return 2;
	}
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0 || sz > 4 * 1024 * 1024) {
		fclose(f);
		return 2;
	}
	json = malloc((size_t)sz + 1);
	if (json == NULL ||
	    fread(json, 1, (size_t)sz, f) != (size_t)sz) {
		fclose(f);
		return 2;
	}
	fclose(f);
	json[sz] = '\\0';
	if (enforce_spec_parse(&spec, json) != 0) {
		fprintf(stderr, "retrace-enforce: spec unparseable\\n");
		return 2;
	}
	free(json);

	rc = enforce_landlock_apply(&spec);
	if (rc == 1)
		fprintf(stderr,
			"retrace-enforce: kernel lacks landlock\\n");
	if (rc < 0) {
		fprintf(stderr,
			"retrace-enforce: landlock apply failed: %s\\n",
			strerror(errno));
		return 2;
	}
	if (rc == 1 && !allow_missing)
		return 2;

	rc = enforce_seccomp_apply(&spec);
	if (rc == 1)
		fprintf(stderr,
			"retrace-enforce: kernel lacks the seccomp floor\\n");
	if (rc < 0) {
		fprintf(stderr,
			"retrace-enforce: seccomp apply failed: %s\\n",
			strerror(errno));
		return 2;
	}
	if (rc == 1 && !allow_missing)
		return 2;

	execvp(argv[i + 1], &argv[i + 1]);
	perror("execvp");
	return 2;
}
