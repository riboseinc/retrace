/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-snap2inside -- snapcraft.yaml -> the declared-set
 * format (TODO.trace-profile/19). Claims-vs-truth at the
 * PACKAGING layer: the snap DECLARED interfaces (plugs) become
 * the inside.json shape that `retrace-profile --inside` grades
 * observed behavior against -- accesses outside the granted
 * interfaces are confinement violations.
 *
 * usage: retrace-snap2inside [-o inside.json] snapcraft.yaml
 *
 * Interface -> declared surface map (v1, honest about scope):
 *   home             -> $HOME/ (read prefix)
 *   removable-media  -> /media/, /run/media/ (read prefixes)
 *   network          -> net (connect/send/recv)
 *   network-bind     -> net (listening)
 *   personal-files / system-files / raw-usb / others: NOT
 *     mapped (their per-snap read/write lists are declared in
 *     the snap's snapd slot config, not in snapcraft.yaml) --
 *     reported as unmapped in the output notes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parson.h"

static void usage(FILE *out)
{
	fprintf(out,
		"Usage: retrace-snap2inside [-o inside.json] snapcraft.yaml\n"
		"\n"
		"Convert a snapcraft.yaml's app plugs to the inside.json\n"
		"declared-set shape for `retrace-profile --inside`.\n");
}

/*
 * Minimal YAML scan for this fixed shape -- we need the
 * `plugs:` list under an app (snapcraft apps are top-level
 * keys with 2-space indentation; plugs is a list item under
 * the app). No general YAML parser (parson is JSON only);
 * the grammar we accept:
 *   apps:
 *     <name>:
 *       plugs:
 *         - home
 *         - network
 * or the compact `plugs: [home, network]` form.
 */
/* parse "plugs: [a, b]" compact list from s into plugs[n..] */
static size_t parse_compact(char *s, char (*plugs)[32], size_t max)
{
	size_t n = 0;

	while (*s != '\0' && *s != ']' && n < max) {
		char *e = s;

		while (*e != '\0' && *e != ',' && *e != ']')
			e++;
		{
			size_t l = (size_t)(e - s);

			if (l > 0 && l < 32) {
				memcpy(plugs[n], s, l);
				plugs[n][l] = '\0';
				n++;
			}
		}
		s = (*e == ',') ? e + 1 : e;
		while (*s == ' ')
			s++;
	}
	return n;
}

static int collect_plugs(FILE *in, char (*plugs)[32], size_t max,
			 size_t *count)
{
	char line[512];
	int in_apps = 0;
	int in_plugs = 0;
	int compact = 0;
	size_t n = 0;

	while (fgets(line, sizeof(line), in) != NULL) {
		char *s = line;
		size_t len = strlen(line);

		while (len > 0 && (line[len - 1] == '\n' ||
				   line[len - 1] == '\r'))
			line[--len] = '\0';
		if (len == 0 || line[0] == '#')
			continue;
		if (strncmp(s, "apps:", 5) == 0) {
			in_apps = 1;
			in_plugs = 0;
			continue;
		}
		if (s[0] != ' ' && s[0] != '\t') {
			/* a new top-level section ends apps */
			if (in_apps)
				break;
			continue;
		}
		if (!in_apps)
			continue;
		while (*s == ' ' || *s == '\t')
			s++;
		if (strncmp(s, "plugs:", 6) == 0) {
			in_plugs = 1;
			s += 6;
			while (*s == ' ')
				s++;
			if (*s == '[') {
				n += parse_compact(s + 1,
					plugs + n, max - n);
				compact = 1;
				in_plugs = 0;
			}
			continue;
		}
		if (in_plugs) {
			while (*s == ' ' || *s == '-' || *s == '\t')
				s++;
			if (*s != '\0' && *s != '#' && n < max) {
				size_t l = strlen(s);

				if (l >= 32)
					l = 31;
				memcpy(plugs[n], s, l);
				plugs[n][l] = '\0';
				n++;
			}
		}
	}
	(void)compact;
	*count = n;
	return n > 0 ? 0 : -1;
}

struct iface_map {
	const char *plug;
	const char *path;   /* NULL = no path surface */
	int is_net;
	const char *note;   /* unmapped plugs get a note */
};

static const struct iface_map g_map[] = {
	/*
	 * the home interface maps to the CONCRETE home at conversion
	 * time (a literal "$HOME/" would never match an observed
	 * absolute path and would over-report violations)
	 */
	{ "home", NULL, 0, NULL },  /* expanded at runtime */
	{ "removable-media", "/media/", 0, NULL },
	{ "removable-media", "/run/media/", 0, NULL },
	{ "network", NULL, 1, NULL },
	{ "network-bind", NULL, 1, NULL },
	{ "network-control", NULL, 1, NULL },
	{ NULL, NULL, 0, NULL }
};

int main(int argc, char **argv)
{
	const char *in_path = NULL;
	const char *out_path = NULL;
	FILE *in;
	FILE *out = stdout;
	char plugs[32][32];
	size_t count = 0;
	size_t i;
	JSON_Value *root;
	JSON_Object *root_o;
	JSON_Object *profile_o;
	JSON_Value *accesses;
	JSON_Value *net_arr;
	JSON_Value *notes;
	JSON_Value *pv;
	char *serialized;
	int mapped_any = 0;

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
	in = fopen(in_path, "r");
	if (in == NULL) {
		perror(in_path);
		return 2;
	}
	if (collect_plugs(in, plugs, 32, &count) != 0) {
		fprintf(stderr,
"retrace-snap2inside: no plugs found (need apps:<app>:plugs)\n");
		fclose(in);
		return 1;
	}
	fclose(in);

	root = json_value_init_object();
	root_o = json_value_get_object(root);
	pv = json_value_init_object();
	profile_o = json_value_get_object(pv);
	accesses = json_value_init_array();
	net_arr = json_value_init_array();
	notes = json_value_init_array();

	for (i = 0; i < count; i++) {
		const struct iface_map *m = g_map;
		int hit = 0;

		for (; m->plug != NULL; m++) {
			if (strcmp(plugs[i], m->plug) == 0) {
				hit = 1;
				if (strcmp(m->plug, "home") == 0) {
					/* concrete home path */
					const char *home = getenv(
						"SNAP2INSIDE_HOME");
					JSON_Value *a;

					if (home == NULL)
						home = getenv("HOME");
					a = json_value_init_object();
					json_object_set_string(
						json_value_get_object(a),
						"path", home != NULL ?
						home : "/home/");
					json_object_set_string(
						json_value_get_object(a),
						"class", "read");
					json_object_set_number(
						json_value_get_object(a),
						"hits", 1);
					json_array_append_value(
						json_value_get_array(
							accesses), a);
				} else if (m->is_net) {
					json_array_append_string(
						json_value_get_array(net_arr),
						"*");
				} else if (m->path != NULL) {
					JSON_Value *a =
						json_value_init_object();

					json_object_set_string(
						json_value_get_object(a),
						"path", m->path);
					json_object_set_string(
						json_value_get_object(a),
						"class", "read");
					json_object_set_number(
						json_value_get_object(a),
						"hits", 1);
					json_array_append_value(
						json_value_get_array(
							accesses), a);
				}
			}
		}
		if (hit) {
			mapped_any = 1;
		} else {
			/* unmapped interfaces are honest notes, not
			 * silent drops
			 */
			json_array_append_string(
				json_value_get_array(notes), plugs[i]);
		}
	}

	json_object_set_value(profile_o, "accesses", accesses);
	json_object_set_value(profile_o, "net", net_arr);
	json_object_set_value(root_o, "profile", pv);
	json_object_set_value(root_o, "notes_unmapped_interfaces",
		notes);

	serialized = json_serialize_to_string_pretty(root);
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

	fprintf(stderr,
"retrace-snap2inside: %zu plugs (%s)%s\n", count,
		mapped_any ? "mapped" : "none mapped",
		json_array_get_count(json_value_get_array(notes)) > 0 ?
		"; unmapped listed in notes_unmapped_interfaces" : "");
	return 0;
}
