/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Bump-allocator. See arena.h.
 */
#include "arena.h"
#include "internal_util.h"

#include <stdlib.h>
#include <string.h>

otlp_status_t
otlp_arena_init(struct otlp_arena *a)
{
	a->buf	 = a->initial_inline;
	a->len	 = 0;
	a->cap	 = sizeof(a->initial_inline);
	a->owns_buf = false;
	return OTLP_OK;
}

void
otlp_arena_free(struct otlp_arena *a)
{
	if (!a)
		return;
	if (a->owns_buf)
		otlp_free(a->buf);
	a->buf	 = NULL;
	a->len	 = 0;
	a->cap	 = 0;
	a->owns_buf = false;
}

static otlp_status_t
grow_to(struct otlp_arena *a, size_t need)
{
	size_t new_cap;
	uint8_t *p;

	while (a->cap < need) {
		if (a->cap > (SIZE_MAX / 2))
			return OTLP_ERR_NOMEM;
		new_cap = a->cap ? a->cap * 2 : 64;
		if (a->cap == 0)
			new_cap = 64;
		a->cap = new_cap;
	}

	if (a->cap <= sizeof(a->initial_inline))
		return OTLP_OK;  /* still inline */

	p = otlp_realloc(a->owns_buf ? a->buf : NULL, a->cap);
	if (!p)
		return OTLP_ERR_NOMEM;
	if (!a->owns_buf) {
		memcpy(p, a->initial_inline, a->len);
		a->buf = p;
	} else {
		a->buf = p;
	}
	a->owns_buf = true;
	return OTLP_OK;
}

otlp_status_t
otlp_arena_append(struct otlp_arena *a, const uint8_t *src, size_t len)
{
	otlp_status_t st;

	if (len == 0)
		return OTLP_OK;
	st = grow_to(a, a->len + len);
	if (st != OTLP_OK)
		return st;
	if (src)
		memcpy(a->buf + a->len, src, len);
	else
		memset(a->buf + a->len, 0, len);
	a->len += len;
	return OTLP_OK;
}

otlp_status_t
otlp_arena_zero_extend(struct otlp_arena *a, size_t len, uint8_t **out)
{
	otlp_status_t st;

	st = grow_to(a, a->len + len);
	if (st != OTLP_OK)
		return st;
	*out = a->buf + a->len;
	memset(*out, 0, len);
	a->len += len;
	return OTLP_OK;
}
