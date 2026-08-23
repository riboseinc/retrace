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

int fuzz_dict_load(fuzz_dict_t *d, const char *path)
{
	char *raw;
	long sz;
	size_t got;
	FILE *f;
	size_t i;

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
	retrace_real_impls.free(raw);
	return 0;
}

const char *fuzz_dict_pick(const fuzz_dict_t *d)
{
	if (d->count <= 0)
		return NULL;
	return d->tokens[rand() % d->count];
}
