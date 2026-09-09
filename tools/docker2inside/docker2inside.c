/*
 * Copyright (c) 2017, [Ribose Inc](https://ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-docker2inside -- docker/OCI image -> the declared-set
 * format (TODO.impl/02). The image's own filesystem IS the
 * declared surface; grading observed behavior against it
 * answers "did this container stay inside its image?".
 *
 * usage: retrace-docker2inside [-o inside.json] image.tar
 *
 * Input: a `docker save` tarball (uncompressed, with its
 * manifest.json -- the form docker/podman/containerd all
 * write). Layers apply in the manifest's order (lowest first);
 * AUFS overlay semantics:
 *   <dir>/.wh.<name>   deletes <name> (and its children)
 *   <dir>/.wh..wh..opq empties <dir>'s prior children
 * Long names ride GNU L/K and PAX 'x' path records -- both are
 * honored (go's archive/tar emits them for real images).
 *
 * Every surviving path lands in the inside.json shape: dirs as
 * read-prefixes, files exact, sorted (stable diffs). v1 reads
 * the filesystem layers only; the config blob's ExposedPorts
 * and HEALTHCHECK land in a later slice (never dropped:
 * notes carry the layer and whiteout counts).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parson.h"

#define PATH_SET_CAP 65536	/* power of two */
#define PATH_MAX_LEN 512

struct path_set {
	char *slots[PATH_SET_CAP];
	size_t count;
};

/* FNV-1a (the project's chain hash -- one idiom) */
static unsigned long fnv(const char *s)
{
	unsigned long h = 1469598103934665603UL;

	while (*s != '\0') {
		h ^= (unsigned char)*s++;
		h *= 1099511628211UL;
	}
	return h;
}

static int set_add(struct path_set *set, const char *path)
{
	unsigned long i = fnv(path) & (PATH_SET_CAP - 1);

	while (set->slots[i] != NULL) {
		if (strcmp(set->slots[i], path) == 0)
			return 0;	/* already there */
		i = (i + 1) & (PATH_SET_CAP - 1);
	}
	set->slots[i] = strdup(path);
	if (set->slots[i] == NULL)
		return -1;
	set->count++;
	return 0;
}

static int set_has(struct path_set *set, const char *path)
{
	unsigned long i = fnv(path) & (PATH_SET_CAP - 1);

	while (set->slots[i] != NULL) {
		if (strcmp(set->slots[i], path) == 0)
			return 1;
		i = (i + 1) & (PATH_SET_CAP - 1);
	}
	return 0;
}

/* remove path and any children (path + "/...") */
static void set_remove(struct path_set *set, const char *path)
{
	size_t n = strlen(path);
	unsigned long i;

	for (i = 0; i < PATH_SET_CAP; i++) {
		if (set->slots[i] == NULL)
			continue;
		if (strncmp(set->slots[i], path, n) == 0 &&
		    (set->slots[i][n] == '\0' || set->slots[i][n] == '/')) {
			free(set->slots[i]);
			set->slots[i] = NULL;
			set->count--;
		}
	}
}

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

static char **set_sorted(struct path_set *set)
{
	char **out = calloc(set->count ? set->count : 1,
		sizeof(char *));
	size_t n = 0;
	size_t i;

	if (out == NULL)
		return NULL;
	for (i = 0; i < PATH_SET_CAP; i++) {
		if (set->slots[i] != NULL)
			out[n++] = set->slots[i];
	}
	qsort(out, n, sizeof(char *), cmp_str);
	return out;
}

/* ---------- minimal tar reader ---------- */

struct tar_reader {
	FILE *f;
	unsigned char header[512];
	char name[PATH_MAX_LEN];	/* after L/K + PAX */
	long size;
	char typeflag;
	char pax_path[PATH_MAX_LEN];	/* pending PAX path= */
};

static long tar_octal(const char *p, size_t n)
{
	long v = 0;
	size_t i;

	for (i = 0; i < n; i++) {
		if (p[i] < '0' || p[i] > '7')
			break;
		v = v * 8 + (p[i] - '0');
	}
	return v;
}

/* returns 1 with name/size/typeflag set for the next member,
 * 0 at end of archive, -1 on error
 */
static int tar_next(struct tar_reader *t)
{
	long to_skip;
	size_t got;
	int using_pax_path = 0;

	if (t->pax_path[0] != '\0')
		using_pax_path = 1;
	for (;;) {
		got = fread(t->header, 1, 512, t->f);
		if (got != 512)
			return got == 0 ? 0 : -1;
		if (t->header[0] == '\0')
			return 0;	/* end block */
		t->size = tar_octal((const char *)t->header + 124,
			11);
		t->typeflag = (char)t->header[156];
		{
			char prefix[156];
			size_t plen = 0;

			memcpy(prefix, t->header + 345, 155);
			prefix[155] = '\0';
			plen = strlen(prefix);
			if (plen > 0 &&
			    t->typeflag != 'L' && t->typeflag != 'K') {
				snprintf(t->name,
					sizeof(t->name), "%.*s/%.*s",
					(int)plen, prefix,
					99, (const char *)t->header);
			} else {
				snprintf(t->name, sizeof(t->name),
					"%.*s", 99,
					(const char *)t->header);
			}
		}
		if (t->typeflag == 'L' || t->typeflag == 'K') {
			/* GNU long name: the data IS the real name */
			char longname[PATH_MAX_LEN];
			size_t n = t->size < (long)sizeof(longname) - 1 ?
				(size_t)t->size : sizeof(longname) - 1;

			if (fread(longname, 1, n, t->f) != n)
				return -1;
			longname[n] = '\0';
			snprintf(t->name, sizeof(t->name), "%s",
				longname);
			to_skip = ((t->size + 511) / 512) * 512 -
				(long)n;
			if (to_skip > 0 &&
			    fseek(t->f, to_skip, SEEK_CUR) != 0)
				return -1;
			continue;	/* next header carries the type */
		}
		if (t->typeflag == 'x' || t->typeflag == 'g') {
			/* PAX: scan records for path= */
			char pax[4096];
			size_t n = t->size < (long)sizeof(pax) ?
				(size_t)t->size : sizeof(pax);
			char *p;

			if (fread(pax, 1, n, t->f) != n)
				return -1;
			pax[n] = '\0';
			p = strstr(pax, " path=");
			if (p != NULL) {
				char *v = p + 6;
				char *eol = strchr(v, '\n');

				if (eol != NULL) {
					*eol = '\0';
					snprintf(t->pax_path,
						sizeof(t->pax_path), "%s",
						v);
					using_pax_path = 1;
				}
			}
			to_skip = ((t->size + 511) / 512) * 512 -
				(long)n;
			if (to_skip > 0 &&
			    fseek(t->f, to_skip, SEEK_CUR) != 0)
				return -1;
			continue;
		}
		if (using_pax_path) {
			snprintf(t->name, sizeof(t->name), "%s",
				t->pax_path);
			t->pax_path[0] = '\0';
		}
		return 1;
	}
}

static void tar_skip_data(struct tar_reader *t)
{
	long pad = ((t->size + 511) / 512) * 512 - t->size;

	if (t->size > 0 && fseek(t->f, t->size, SEEK_CUR) != 0)
		return;
	if (pad > 0)
		(void)fseek(t->f, pad, SEEK_CUR);
}

static void normalize(char *path)
{
	char *src = path;
	char *dst = path;

	while (*src == '.' && src[1] == '/')
		src += 2;
	while (*src != '\0') {
		if (src[0] == '/' && src[1] == '/') {
			src++;
			continue;
		}
		*dst++ = *src++;
	}
	*dst = '\0';
}

/* apply one layer tar (already positioned at its start) */
static int apply_layer(FILE *f, struct path_set *set,
	long *whiteouts)
{
	struct tar_reader t;
	int r;

	memset(&t, 0, sizeof(t));
	t.f = f;
	while ((r = tar_next(&t)) == 1) {
		char path[PATH_MAX_LEN];
		const char *base;
		const char *wh;

		if (t.size < 0)
			return -1;
		normalize(t.name);
		if (t.name[0] == '\0') {
			tar_skip_data(&t);
			continue;
		}
		base = strrchr(t.name, '/');
		base = base != NULL ? base + 1 : t.name;
		wh = strncmp(base, ".wh.", 4) == 0 ? base + 4 : NULL;

		if (wh != NULL && strcmp(base, ".wh..wh..opq") == 0) {
			/* opaque: drop prior children of this dir */
			char dir[PATH_MAX_LEN];
			size_t dl = (size_t)(base - t.name);

			memcpy(dir, t.name, dl);
			if (dl > 0 && dir[dl - 1] == '/')
				dl--;
			dir[dl] = '\0';
			set_remove(set, dir);
			(*whiteouts)++;
		} else if (wh != NULL) {
			char victim[PATH_MAX_LEN];
			size_t dl = (size_t)(base - t.name);

			memcpy(victim, t.name, dl);
			victim[dl] = '\0';
			snprintf(victim + dl, sizeof(victim) - dl,
				"%s", wh);
			set_remove(set, victim);
			(*whiteouts)++;
		} else if (t.typeflag == '5' ||
			   t.name[strlen(t.name) - 1] == '/') {
			/* dirs are prefix entries; never double the / */
			size_t nl = strlen(t.name);

			if (nl > 0 && t.name[nl - 1] == '/') {
				snprintf(path, sizeof(path), "%s",
					t.name);
			} else {
				snprintf(path, sizeof(path), "%s/",
					t.name);
			}
			if (set_add(set, path) != 0)
				return -1;
		} else if (t.typeflag == '0' || t.typeflag == '\0' ||
			   t.typeflag == '2' || t.typeflag == '1') {
			if (set_add(set, t.name) != 0)
				return -1;
		}
		/* devices/fifos are not declared-set material */
		tar_skip_data(&t);
	}
	return r < 0 ? -1 : 0;
}

static void usage(FILE *out)
{
	fprintf(out,
		"Usage: retrace-docker2inside [-o inside.json] image.tar\n"
		"\n"
		"Convert a docker save tarball (with manifest.json) to\n"
		"the inside.json declared-set for --inside grading.\n");
}

int main(int argc, char **argv)
{
	const char *in_path = NULL;
	const char *out_path = NULL;
	FILE *in = NULL;
	FILE *out = stdout;
	FILE *layer = NULL;
	struct path_set set;
	JSON_Value *root, *pv, *accesses, *notes;
	JSON_Object *root_o, *profile_o;
	char **sorted = NULL;
	char *manifest_text = NULL;
	long manifest_size = 0;
	long whiteouts = 0;
	int layers = 0;
	size_t i;
	int rc = 1;

	memset(&set, 0, sizeof(set));
	{
		int i;

		for (i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
				out_path = argv[++i];
			else if (argv[i][0] != '-')
				in_path = argv[i];
			else {
				usage(stderr);
				return 2;
			}
		}
	}
	if (in_path == NULL) {
		usage(stderr);
		return 2;
	}
	if (out_path != NULL) {
		out = fopen(out_path, "w");
		if (out == NULL) {
			perror("open output");
			return 2;
		}
	}
	in = fopen(in_path, "rb");
	if (in == NULL) {
		perror("open image tar");
		return 2;
	}

	/* pass 1: find and read manifest.json; remember layer data
	 * offsets + sizes at discovery (the header is consumed
	 * here; ftell lands on the data)
	 */
	{
		struct tar_reader t;
		int r;
		struct {
			long off;
			long size;
			char name[PATH_MAX_LEN];
		} layer_refs[256];
		int n_layer_refs = 0;

		memset(&t, 0, sizeof(t));
		t.f = in;
		while ((r = tar_next(&t)) == 1) {
			const char *nm = t.name;

			if (strcmp(nm, "manifest.json") == 0) {
				manifest_size = t.size;
				manifest_text = malloc(
					(size_t)t.size + 1);
				if (manifest_text == NULL ||
				    fread(manifest_text, 1,
					      (size_t)t.size,
					      in) != (size_t)t.size) {
					fprintf(stderr,
						"docker2inside: manifest.json unreadable\n");
					goto out;
				}
				manifest_text[t.size] = '\0';
				tar_skip_data(&t);
				continue;
			}
			if (n_layer_refs < 256 &&
			    strlen(nm) > 10 &&
			    strcmp(nm + strlen(nm) - 10,
				    "/layer.tar") == 0) {
				layer_refs[n_layer_refs].off = ftell(in);
				layer_refs[n_layer_refs].size = t.size;
				snprintf(layer_refs[n_layer_refs].name,
					PATH_MAX_LEN, "%s", nm);
				n_layer_refs++;
			}
			tar_skip_data(&t);
		}
		if (manifest_text == NULL) {
			fprintf(stderr,
				"docker2inside: no manifest.json (docker save -o form required; OCI layout dirs are a later slice)\n");
			goto out;
		}

		/* pass 2: layers in the MANIFEST's order */
		{
			JSON_Value *mv = json_parse_string(manifest_text);
			JSON_Array *ml;

			if (mv == NULL ||
			    json_array_get_count(
				json_value_get_array(mv)) < 1) {
				fprintf(stderr,
					"docker2inside: manifest.json parse failed\n");
				if (mv != NULL)
					json_value_free(mv);
				goto out;
			}
			ml = json_object_get_array(
				json_array_get_object(
					json_value_get_array(mv), 0),
				"Layers");
			if (ml == NULL ||
			    json_array_get_count(ml) == 0) {
				fprintf(stderr,
					"docker2inside: manifest carries no layers\n");
				if (mv != NULL)
					json_value_free(mv);
				goto out;
			}
			for (i = 0; i < json_array_get_count(ml);
			     i++) {
				const char *want =
					json_array_get_string(ml, i);
				int j;

				for (j = 0; j < n_layer_refs; j++) {
					if (strcmp(layer_refs[j].name,
						    want) == 0)
						break;
				}
				if (j >= n_layer_refs) {
					fprintf(stderr,
						"docker2inside: layer %s missing from the tar\n",
						want);
					json_value_free(mv);
					goto out;
				}
				if (fseek(in, layer_refs[j].off,
					    SEEK_SET) != 0) {
					json_value_free(mv);
					goto out;
				}
				layer = tmpfile();
				if (layer == NULL) {
					json_value_free(mv);
					goto out;
				}
				{
					/* copy the layer's data bytes,
					 * then apply from the copy
					 */
					char buf[8192];
					long left = layer_refs[j].size;

					while (left > 0) {
						size_t n = left > (long)sizeof(buf) ?
							sizeof(buf) :
							(size_t)left;

						if (fread(buf, 1, n,
							    in) != n) {
							json_value_free(mv);
							goto out;
						}
						if (fwrite(buf, 1, n,
							    layer) != n) {
							json_value_free(mv);
							goto out;
						}
						left -= (long)n;
					}
					rewind(layer);
				}
				if (apply_layer(layer, &set,
					    &whiteouts) != 0) {
					fprintf(stderr,
						"docker2inside: layer %s malformed\n",
						want);
					json_value_free(mv);
					goto out;
				}
				fclose(layer);
				layer = NULL;
				layers++;
			}
			json_value_free(mv);
		}
	}

	/* emit the inside.json shape (flatpak2inside's envelope) */
	root = json_value_init_object();
	root_o = json_value_get_object(root);
	pv = json_value_init_object();
	profile_o = json_value_get_object(pv);
	accesses = json_value_init_array();
	notes = json_value_init_array();

	sorted = set_sorted(&set);
	if (sorted == NULL)
		goto out;
	for (i = 0; i < set.count; i++) {
		JSON_Value *a = json_value_init_object();

		json_object_set_string(json_value_get_object(a),
			"path", sorted[i]);
		json_object_set_string(json_value_get_object(a),
			"class", "read");
		json_object_set_number(json_value_get_object(a),
			"hits", 1);
		json_array_append_value(json_value_get_array(
			accesses), a);
	}
	json_object_set_value(profile_o, "accesses", accesses);
	json_object_set_value(profile_o, "net", json_value_init_array());
	{
		char note[96];

		snprintf(note, sizeof(note),
			"layers_applied=%d whiteouts=%d paths=%zu",
			layers, (int)whiteouts, set.count);
		json_array_append_string(json_value_get_array(notes),
			note);
	}
	json_object_set_value(root_o, "profile", pv);
	json_object_set_value(root_o, "notes", notes);
	{
		char *serialized =
			json_serialize_to_string_pretty(root);

		if (serialized != NULL) {
			fputs(serialized, out);
			fputs("\n", out);
			json_free_serialized_string(serialized);
			rc = 0;
		}
		json_value_free(root);
	}
out:
	if (sorted != NULL)
		free(sorted);
	if (layer != NULL)
		fclose(layer);
	if (manifest_text != NULL)
		free(manifest_text);
	for (i = 0; i < PATH_SET_CAP; i++)
		free(set.slots[i]);
	if (in != NULL)
		fclose(in);
	if (out != NULL && out != stdout)
		fclose(out);
	return rc;
}
