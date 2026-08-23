/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "converter.h"
#include "parson.h"

static void usage(FILE *out, const struct converter_app *app)
{
	fprintf(out, "Usage: %s [-o out.json] <input>\n\n%s\n"
		"Output goes to stdout unless -o is given.\n",
		app->name, app->usage);
}

int converter_main(int argc, char **argv,
	const struct converter_app *app)
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
			usage(stdout, app);
			return 0;
		} else if (argv[i][0] != '-' || argv[i][1] == '\0')
			in_path = argv[i];
		else {
			usage(stderr, app);
			return 2;
		}
	}
	if (in_path == NULL) {
		usage(stderr, app);
		return 2;
	}

	in = fopen(in_path, "r");
	if (in == NULL) {
		perror(in_path);
		return 2;
	}

	root = json_value_init_array();
	arr = json_value_get_array(root);
	converted = app->convert(in, arr);
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

	fprintf(stderr, "%s: %d %s converted\n",
		app->name, converted, app->row_noun);
	return 0;
}
