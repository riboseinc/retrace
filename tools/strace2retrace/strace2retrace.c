/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-strace2retrace -- the Linux kernel-truth converter
 * (TODO.windows/08). Feeds retrace-profile --kernel (and
 * retrace-correlate --outside) from an strace file capture.
 *
 * usage: retrace-strace2retrace [-o out.json] strace.log
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "convert.h"

static void usage(FILE *out)
{
	fprintf(out,
		"Usage: retrace-strace2retrace [-o out.json] strace.log\n"
		"\n"
		"Convert `strace -f -e trace=%%file -o strace.log ./app`\n"
		"output to a retrace trace document (kernel-layer truth).\n"
		"Output goes to stdout unless -o is given.\n");
}

int main(int argc, char **argv)
{
	const char *in_path = NULL;
	const char *out_path = NULL;
	FILE *in = stdin;
	FILE *out = stdout;
	JSON_Value *root;
	JSON_Array *arr;
	char *serialized;
	int converted;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
			out_path = argv[++i];
		else if (strcmp(argv[i], "-h") == 0 ||
			 strcmp(argv[i], "--help") == 0) {
			usage(stdout);
			return 0;
		} else if (argv[i][0] != '-' || argv[i][1] == '\0') {
			in_path = argv[i];
		} else {
			usage(stderr);
			return 2;
		}
	}
	if (in_path == NULL) {
		usage(stderr);
		return 2;
	}

	in = fopen(in_path, "r");
	if (in == NULL) {
		perror(in_path);
		return 2;
	}

	root = json_value_init_array();
	arr = json_value_get_array(root);
	converted = strace_convert(in, arr);
	fclose(in);

	serialized = json_serialize_to_string(root);
	if (out_path != NULL) {
		out = fopen(out_path, "w");
		if (out == NULL) {
			perror(out_path);
			return 2;
		}
	}
	fprintf(out, "%s\n", serialized);
	if (out != stdout)
		fclose(out);

	json_free_serialized_string(serialized);
	json_value_free(root);

	fprintf(stderr, "retrace-strace2retrace: %d syscall lines converted\n",
		converted);
	return 0;
}
