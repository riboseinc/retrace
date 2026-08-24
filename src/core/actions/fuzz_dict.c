/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "real_impls.h"

#include "fuzz_dict.h"

/*
 * Expand one '@'-template: %N% -> the Nth flat token (1-based).
 * Returns 0 on success; -1 on an out-of-range reference or
 * overflow. Templates may only reference FLAT tokens (the
 * tokens list at load time contains only flat entries).
 */
static int expand_template(const char *tmpl, char *out,
	size_t out_max, const fuzz_dict_t *d, size_t flat_count)
{
	size_t o = 0;
	const char *p = tmpl;

	while (*p != '\0') {
		if (p[0] == '%' && p[1] >= '1' && p[1] <= '9' &&
		    p[2] == '%') {
			size_t idx = (size_t)(p[1] - '1');
			const char *tok;
			size_t l;

			if (idx >= flat_count)
				return -1;
			tok = d->tokens[idx];
			l = strlen(tok);
			if (o + l >= out_max)
				return -1;
			memcpy(out + o, tok, l);
			o += l;
			p += 3;
		} else {
			if (o + 1 >= out_max)
				return -1;
			out[o++] = *p++;
		}
	}
	out[o] = '\0';
	return 0;
}

int fuzz_dict_load(fuzz_dict_t *d, const char *path)
{
	char *raw;
	long sz;
	size_t got;
	FILE *f;
	size_t i;
	char templates[FUZZ_DICT_MAX][FUZZ_DICT_TOKEN_MAX];
	size_t tmpl_n = 0;
	size_t flat_count;

	memset(templates, 0, sizeof(templates));
	d->count = 0;
	f = retrace_real_impls.fopen(path, "rb");
	if (f == NULL)
		return -1;
	if (retrace_real_impls.fseek(f, 0, SEEK_END) != 0) {
		retrace_real_impls.fclose(f);
		return -1;
	}
	sz = retrace_real_impls.ftell(f);
	if (sz < 0) {
		retrace_real_impls.fclose(f);
		return -1;
	}
	retrace_real_impls.fseek(f, 0, SEEK_SET);
	if (sz > FUZZ_DICT_MAX * FUZZ_DICT_TOKEN_MAX)
		sz = FUZZ_DICT_MAX * FUZZ_DICT_TOKEN_MAX;

	raw = retrace_real_impls.malloc((size_t)sz + 1);
	if (raw == NULL) {
		retrace_real_impls.fclose(f);
		return -1;
	}
	got = retrace_real_impls.fread(raw, 1, (size_t)sz, f);
	retrace_real_impls.fclose(f);
	raw[got] = '\0';

	/* line split; '#'-comment, blank, and edge-whitespace skip */
	for (i = 0; i < got && d->count < FUZZ_DICT_MAX; ) {
		const char *line = raw + i;
		const char *nl = line;
		size_t len;
		size_t lead = 0;

		while (*nl != '\0' && *nl != '\n')
			nl++;
		len = (size_t)(nl - line);
		i += len + 1;

		while (lead < len && (line[lead] == ' ' ||
				      line[lead] == '\t' ||
				      line[lead] == '\r'))
			lead++;
		if (lead == len || line[lead] == '#')
			continue;
		if (line[lead] == '@') {
			/* template: stash, expand after the scan (the
			 * referenced flat tokens must all exist first)
			 */
			if (tmpl_n < FUZZ_DICT_MAX) {
				size_t l = len - 1;

				if (l >= FUZZ_DICT_TOKEN_MAX)
					l = FUZZ_DICT_TOKEN_MAX - 1;
				memcpy(templates[tmpl_n], line + lead + 1,
					l);
				templates[tmpl_n][l] = '\0';
				tmpl_n++;
			}
			continue;
		}
		len -= lead;
		while (len > 0 && (line[lead + len - 1] == '\r' ||
				   line[lead + len - 1] == ' ' ||
				   line[lead + len - 1] == '\t'))
			len--;
		if (len == 0)
			continue;
		if (len >= FUZZ_DICT_TOKEN_MAX)
			len = FUZZ_DICT_TOKEN_MAX - 1;
		memcpy(d->tokens[d->count], line + lead, len);
		d->tokens[d->count][len] = '\0';
		d->count++;
	}
	flat_count = d->count;
	for (i = 0; i < tmpl_n; i++) {
		char expanded[FUZZ_DICT_TOKEN_MAX];

		if (d->count >= FUZZ_DICT_MAX)
			break;
		if (expand_template(templates[i], expanded,
			sizeof(expanded), d, flat_count) != 0) {
			d->count = 0;
			retrace_real_impls.free(raw);
			return -1;
		}
		memcpy(d->tokens[d->count], expanded,
			strlen(expanded) + 1);
		d->count++;
	}
	retrace_real_impls.free(raw);
	return 0;
}

const char *fuzz_dict_pick(const fuzz_dict_t *d)
{
	if (d->count <= 0)
		return NULL;
	return d->tokens[rand() % d->count];
}
