/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-flatpak2inside -- flatpak manifest -> the declared-set
 * format (TODO.trace-profile/19). finish-args are the flatpak's
 * DECLARED surface; grading observed behavior against them
 * reports sandbox escapes.
 *
 * usage: retrace-flatpak2inside [-o inside.json] manifest.json
 *
 * Mapped finish-args (JSON manifests only in v1; the YAML form
 * is honestly refused):
 *   --filesystem=host          -> "/" (whole root, read)
 *   --filesystem=home          -> "$HOME/"
 *   --filesystem=<path>        -> <path> (prefix)
 *   --filesystem=<path>:ro     -> <path> (read class)
 *   --filesystem=<path>:rw     -> <path> (write class)
 *   --share=network            -> net
 *   --device=all               -> /dev/ (read)
 * Others land in notes_unmapped_args (never dropped silently).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parson.h"

static void usage(FILE *out)
{
	fprintf(out,
		"Usage: retrace-flatpak2inside [-o inside.json] manifest.json\n"
		"\n"
		"Convert a flatpak JSON manifest's finish-args to the\n"
		"inside.json declared-set shape for --inside grading.\n");
}

int main(int argc, char **argv)
{
	const char *in_path = NULL;
	const char *out_path = NULL;
	FILE *out = stdout;
	JSON_Value *root;
	JSON_Object *root_o;
	JSON_Array *finish;
	JSON_Value *pv;
	JSON_Object *profile_o;
	JSON_Value *accesses;
	JSON_Value *net_arr;
	JSON_Value *notes;
	char *serialized;
	char *text;
	size_t fsize;
	FILE *in;
	size_t i;
	int mapped = 0;

	for (i = 1; i < (size_t)argc; i++) {
		if (strcmp(argv[i], "-o") == 0 && i + 1 < (size_t)argc)
			out_path = argv[++i];
		else if (strcmp(argv[i], "-h") == 0 ||
			 strcmp(argv[i], "--help") == 0) {
			usage(stdout);
			return 0;
		} else if (argv[i][0] != '-') {
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
	in = fopen(in_path, "rb");
	if (in == NULL) {
		perror(in_path);
		return 2;
	}
	fseek(in, 0, SEEK_END);
	fsize = (size_t)ftell(in);
	fseek(in, 0, SEEK_SET);
	text = (char *)malloc(fsize + 1);
	if (text == NULL ||
	    fread(text, 1, fsize, in) != fsize) {
		fclose(in);
		return 2;
	}
	text[fsize] = '\0';
	fclose(in);

	root = json_parse_string_with_comments(text);
	free(text);
	if (root == NULL ||
	    json_value_get_object(root) == NULL) {
		fprintf(stderr,
"retrace-flatpak2inside: not JSON (v1 accepts JSON only)\n");
		return 1;
	}
	root_o = json_value_get_object(root);
	finish = json_object_get_array(root_o, "finish-args");
	if (finish == NULL) {
		fprintf(stderr,
"retrace-flatpak2inside: no finish-args array -- nothing declared\n");
		return 1;
	}

	pv = json_value_init_object();
	profile_o = json_value_get_object(pv);
	accesses = json_value_init_array();
	net_arr = json_value_init_array();
	notes = json_value_init_array();

	for (i = 0; i < json_array_get_count(finish); i++) {
		const char *arg = json_array_get_string(finish, i);

		if (arg == NULL)
			continue;
		if (strncmp(arg, "--filesystem=", 13) == 0) {
			const char *spec = arg + 13;
			const char *colon = strchr(spec, ':');
			char path[512];
			size_t plen = colon != NULL ?
				(size_t)(colon - spec) : strlen(spec);
			const char *cls = "read";

			if (plen >= sizeof(path))
				plen = sizeof(path) - 1;
			memcpy(path, spec, plen);
			path[plen] = '\0';
			if (strcmp(path, "host") == 0)
				strcpy(path, "/");
			else if (strcmp(path, "home") == 0)
				strcpy(path, "$HOME/");
			if (colon != NULL && strcmp(colon + 1, "rw") == 0)
				cls = "write";
			{
				JSON_Value *a = json_value_init_object();

				json_object_set_string(
					json_value_get_object(a),
					"path", path);
				json_object_set_string(
					json_value_get_object(a),
					"class", cls);
				json_object_set_number(
					json_value_get_object(a), "hits", 1);
				json_array_append_value(
					json_value_get_array(accesses), a);
			}
			mapped = 1;
		} else if (strncmp(arg, "--share=network", 15) == 0) {
			json_array_append_string(
				json_value_get_array(net_arr), "*");
			mapped = 1;
		} else if (strncmp(arg, "--device=all", 12) == 0) {
			JSON_Value *a = json_value_init_object();

			json_object_set_string(json_value_get_object(a),
				"path", "/dev/");
			json_object_set_string(json_value_get_object(a),
				"class", "read");
			json_array_append_value(
				json_value_get_array(accesses), a);
			mapped = 1;
		} else if (arg[0] == '-') {
			json_array_append_string(
				json_value_get_array(notes), arg);
		}
	}

	json_object_set_value(profile_o, "accesses", accesses);
	json_object_set_value(profile_o, "net", net_arr);
	{
		JSON_Value *out_root = json_value_init_object();
		JSON_Object *out_root_o = json_value_get_object(out_root);

		json_object_set_value(out_root_o, "profile", pv);
		json_object_set_value(out_root_o,
			"notes_unmapped_args", notes);
		serialized = json_serialize_to_string_pretty(out_root);
		json_value_free(out_root);
	}

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

	fprintf(stderr, "retrace-flatpak2inside: %s\n",
		mapped ? "mapped finish-args to the declared set" :
		"no mappable finish-args found");
	return 0;
}
