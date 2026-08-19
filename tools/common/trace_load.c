/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "trace_load.h"
#include "stream.h"

#include <stdio.h>
#include <stdlib.h>

struct Ctx {
	JSON_Array *dst;
};

static void
collect(JSON_Object *entry, void *opaque)
{
	struct Ctx *ctx = (struct Ctx *)opaque;
	JSON_Value *copy = json_value_deep_copy(
		json_object_get_wrapping_value(entry));

	/*
	 * The scanner reuses and frees the entry after the
	 * callback; only the deep copy is kept.
	 */
	if (copy != NULL)
		(void)json_array_append_value(ctx->dst, copy);
}

JSON_Value *trace_load_file(const char *path, size_t *skipped)
{
	FILE *f = fopen(path, "rb");
	long sz;
	char *buf;
	JSON_Value *arr = NULL;
	struct Ctx ctx;

	if (skipped != NULL)
		*skipped = 0;
	if (f == NULL)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0)
		goto out;
	sz = ftell(f);
	if (sz < 0)
		goto out;
	if (fseek(f, 0, SEEK_SET) != 0)
		goto out;
	buf = (char *)malloc((size_t)sz + 1);
	if (buf == NULL)
		goto out;
	if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		goto out;
	}
	fclose(f);
	f = NULL;
	buf[sz] = '\0';

	arr = json_value_init_array();
	if (arr == NULL) {
		free(buf);
		return NULL;
	}
	ctx.dst = json_value_get_array(arr);
	(void)corr_stream_scan(buf, (size_t)sz, collect, &ctx, skipped);
	free(buf);
	return arr;

out:
	fclose(f);
	return NULL;
}
