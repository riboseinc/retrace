/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-etw2retrace -- the scripted Windows kernel-truth
 * converter (TODO.trace-profile/24). Feeds retrace-profile
 * --kernel (and retrace-correlate --outside) from an ETW
 * capture taken with scripts/win/etw-capture.ps1 (admin).
 * procmon2retrace stays the zero-install path.
 *
 * usage: retrace-etw2retrace [-o out.json] etw-events.jsonl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "convert.h"

static void usage(FILE *out)
{
	fprintf(out,
		"Usage: retrace-etw2retrace [-o out.json] etw-events.jsonl\n"
		"\n"
		"Convert scripts/win/etw-capture.ps1 raw rows to a retrace\n"
		"trace document (kernel-layer truth). Output goes to stdout\n"
		"unless -o is given.\n");
}

int main(int argc, char **argv)
{
	const char *in_path = NULL;
	const char *out_path = NULL;
	FILE *in;
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
	converted = etw_convert(in, arr);
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

	fprintf(stderr, "retrace-etw2retrace: %d ETW rows converted\n",
		converted);
	return 0;
}
