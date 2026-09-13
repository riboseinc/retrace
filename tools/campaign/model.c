/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The campaign model (TODO.impl/09). See model.h.
 */

#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parson.h"

static int
copy_name(char dst[CAMPAIGN_NAME_MAX], const char *src,
	const char *what, char *err, size_t err_cap)
{
	if (src == NULL || src[0] == '\0') {
		snprintf(err, err_cap, "%s: name required", what);
		return -1;
	}
	snprintf(dst, CAMPAIGN_NAME_MAX, "%s", src);
	return 0;
}

int campaign_load(const char *manifest_path, struct campaign *c,
	char *err, size_t err_cap)
{
	JSON_Value *v;
	JSON_Object *root;
	size_t i;

	memset(c, 0, sizeof(*c));
	c->repeats = 1;
	c->concurrency = 1;
	c->timeout_sec = 30;

	v = json_parse_file(manifest_path);
	if (v == NULL) {
		snprintf(err, err_cap, "cannot parse %s", manifest_path);
		return -1;
	}
	root = json_value_get_object(v);
	if (root == NULL) {
		json_value_free(v);
		snprintf(err, err_cap, "manifest root must be an object");
		return -1;
	}

	{
		double d;

		d = json_object_get_number(root, "repeats");
		if (d >= 1)
			c->repeats = (int)d;
		d = json_object_get_number(root, "concurrency");
		if (d >= 1)
			c->concurrency = (int)d;
		d = json_object_get_number(root, "timeout_sec");
		if (d >= 1)
			c->timeout_sec = (int)d;
	}

	{
		const char *s = json_object_get_string(root, "preload");

		if (s != NULL)
			snprintf(c->preload, sizeof(c->preload), "%s", s);
		s = json_object_get_string(root, "ctl_sock");
		if (s != NULL)
			snprintf(c->ctl_sock, sizeof(c->ctl_sock), "%s", s);
		s = json_object_get_string(root, "journal");
		if (s != NULL)
			snprintf(c->journal_base,
				sizeof(c->journal_base), "%s", s);
	}

	{
		JSON_Array *a = json_object_get_array(root, "samples");

		if (a == NULL) {
			json_value_free(v);
			snprintf(err, err_cap, "samples[] required");
			return -1;
		}
		c->n_samples = json_array_get_count(a);
		if (c->n_samples == 0 ||
		    c->n_samples > CAMPAIGN_MAX_SAMPLES) {
			json_value_free(v);
			snprintf(err, err_cap,
				"samples: 1..%d required",
				CAMPAIGN_MAX_SAMPLES);
			return -1;
		}
		for (i = 0; i < c->n_samples; i++) {
			JSON_Object *o = json_array_get_object(a, i);
			JSON_Array *argv_a;
			const char *nm = json_object_get_string(o, "name");
			size_t k;

			if (copy_name(c->samples[i].name, nm, "sample",
				    err, err_cap) != 0) {
				json_value_free(v);
				return -1;
			}
			argv_a = json_object_get_array(o, "argv");
			if (argv_a == NULL ||
			    json_array_get_count(argv_a) == 0) {
				json_value_free(v);
				snprintf(err, err_cap,
					"sample %s: argv[] required",
					c->samples[i].name);
				return -1;
			}
			c->samples[i].argc =
				json_array_get_count(argv_a);
			if (c->samples[i].argc > CAMPAIGN_MAX_ARGV) {
				json_value_free(v);
				snprintf(err, err_cap,
					"sample %s: argv too long",
					c->samples[i].name);
				return -1;
			}
			for (k = 0; k < c->samples[i].argc; k++) {
				const char *arg = json_array_get_string(
					argv_a, k);

				c->samples[i].argv[k] = strdup(
					arg != NULL ? arg : "");
			}
		}
	}

	{
		JSON_Array *a = json_object_get_array(root, "policies");

		if (a == NULL) {
			json_value_free(v);
			snprintf(err, err_cap, "policies[] required");
			return -1;
		}
		c->n_policies = json_array_get_count(a);
		if (c->n_policies == 0 ||
		    c->n_policies > CAMPAIGN_MAX_POLICIES) {
			json_value_free(v);
			snprintf(err, err_cap,
				"policies: 1..%d required",
				CAMPAIGN_MAX_POLICIES);
			return -1;
		}
		for (i = 0; i < c->n_policies; i++) {
			JSON_Object *o = json_array_get_object(a, i);
			const char *nm = json_object_get_string(o, "name");
			const char *path = json_object_get_string(o,
				"path");

			if (copy_name(c->policies[i].name, nm, "policy",
				    err, err_cap) != 0) {
				json_value_free(v);
				return -1;
			}
			if (path == NULL || path[0] == '\0') {
				json_value_free(v);
				snprintf(err, err_cap,
					"policy %s: path required",
					c->policies[i].name);
				return -1;
			}
			snprintf(c->policies[i].path,
				sizeof(c->policies[i].path), "%s", path);
		}
	}

	/* the matrix: sample-major, then policy, then repeat */
	{
		size_t want = c->n_samples * c->n_policies *
			(size_t)c->repeats;

		if (want > sizeof(c->cells) / sizeof(c->cells[0])) {
			json_value_free(v);
			snprintf(err, err_cap,
				"matrix too large (%zu cells)", want);
			return -1;
		}
		for (i = 0; i < c->n_samples; i++) {
			size_t p;

			for (p = 0; p < c->n_policies; p++) {
				int r;

				for (r = 0; r < c->repeats; r++) {
					struct campaign_cell *cell =
						&c->cells[c->n_cells++];

					cell->sample = (int)i;
					cell->policy = (int)p;
					cell->repeat = r;
				}
			}
		}
	}

	json_value_free(v);
	return 0;
}

void campaign_free(struct campaign *c)
{
	size_t i, k;

	for (i = 0; i < c->n_samples; i++)
		for (k = 0; k < c->samples[i].argc; k++)
			free(c->samples[i].argv[k]);
	memset(c, 0, sizeof(*c));
}
