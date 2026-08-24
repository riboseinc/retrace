/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Internal utilities shared across src/ files. See internal_util.h.
 *
 * This file also implements the custom-allocator dispatch: all
 * otlp_malloc/otlp_free/otlp_realloc/otlp_calloc calls route through
 * the global otlp_allocator_t, which defaults to system malloc/free
 * but can be overridden via otlp_set_allocator() (public API).
 */
#include "internal_util.h"

#include <otlp-c/allocator.h>
#include "span_internal.h"

#include <stdlib.h>
#include <string.h>

/* ── Global allocator state ───────────────────────────────────── */

static otlp_allocator_t g_allocator = {
	.alloc = malloc,
	.realloc = realloc,
	.free = free,
};

void
otlp_set_allocator(const otlp_allocator_t *alloc)
{
	if (alloc)
		g_allocator = *alloc;
	else
	{
		g_allocator.alloc = malloc;
		g_allocator.realloc = realloc;
		g_allocator.free = free;
	}
}

const otlp_allocator_t *
otlp_get_allocator(void)
{
	return &g_allocator;
}

/* ── Allocator-backed wrappers ────────────────────────────────── */

void *
otlp_malloc(size_t n)
{
	return g_allocator.alloc(n);
}

void *
otlp_realloc(void *p, size_t n)
{
	return g_allocator.realloc(p, n);
}

void
otlp_free(void *p)
{
	g_allocator.free(p);
}

void *
otlp_calloc(size_t count, size_t size)
{
	size_t total = count * size;
	void *p = g_allocator.alloc(total);

	if (p)
		memset(p, 0, total);
	return p;
}

/* ── String / byte duplication ────────────────────────────────── */

char *
otlp_dup_str(const char *s)
{
	size_t len;
	char *out;

	if (!s)
		return NULL;
	len = strlen(s);
	if (len == SIZE_MAX)
		return NULL; /* len + 1 would overflow */
	out = otlp_malloc(len + 1);
	if (!out)
		return NULL;
	memcpy(out, s, len + 1);
	return out;
}

uint8_t *
otlp_dup_bytes(const uint8_t *src, size_t len)
{
	uint8_t *out;

	if (len == 0)
		return NULL;
	if (!src)
		return NULL;
	out = otlp_malloc(len);
	if (!out)
		return NULL;
	memcpy(out, src, len);
	return out;
}

/* ── Recursive attribute free ─────────────────────────────────── */

void
otlp_attribute_release_value(struct otlp_attribute *a)
{
	size_t i;

	if (!a)
		return;
	switch (a->type)
	{
		case OTLP_ATTR_ARRAY:
			if (a->v.array_val)
			{
				for (i = 0; i < a->v.array_val->n; i++)
					otlp_attribute_free(
						&a->v.array_val->items[i]);
				otlp_free(a->v.array_val->items);
				otlp_free(a->v.array_val);
				a->v.array_val = NULL;
			}
			break;
		case OTLP_ATTR_KVLIST:
			if (a->v.kvlist_val)
			{
				for (i = 0; i < a->v.kvlist_val->n; i++)
				{
					otlp_free(a->v.kvlist_val->entries[i]
							  .key);
					otlp_attribute_free(
						&a->v.kvlist_val->entries[i]
							 .value);
				}
				otlp_free(a->v.kvlist_val->entries);
				otlp_free(a->v.kvlist_val);
				a->v.kvlist_val = NULL;
			}
			break;
		case OTLP_ATTR_STRING:
			otlp_free(a->v.string_val);
			a->v.string_val = NULL;
			break;
		case OTLP_ATTR_BYTES:
			otlp_free(a->v.bytes_val.data);
			a->v.bytes_val.data = NULL;
			a->v.bytes_val.len = 0;
			break;
		default:
			break;
	}
	/* Safe empty state: the slot stays valid for a typed refill
	 * and for the free path. */
	a->type = OTLP_ATTR_STRING;
	a->v.string_val = NULL;
}

void
otlp_attribute_free(struct otlp_attribute *a)
{
	if (!a)
		return;
	otlp_attribute_release_value(a);
	otlp_free(a->key);
	a->key = NULL;
}

/* ── Attribute copy ───────────────────────────────────────────── */

/* Deep-copy one attribute, including a nested array/kvlist tree.
 * Safe on partial failure: on return != OTLP_OK the destination
 * is left in a state otlp_attribute_free can handle (NULL-safe). */
static otlp_status_t
attr_copy_one(struct otlp_attribute *dst, const struct otlp_attribute *src)
{
	size_t i;

	dst->key = NULL;
	dst->type = OTLP_ATTR_STRING;
	dst->v.string_val = NULL;
	if (src->key)
	{
		dst->key = otlp_dup_str(src->key);
		if (!dst->key)
			return OTLP_ERR_NOMEM;
	}
	switch (src->type)
	{
		case OTLP_ATTR_STRING:
			dst->type = OTLP_ATTR_STRING;
			if (src->v.string_val)
			{
				dst->v.string_val =
					otlp_dup_str(src->v.string_val);
				if (!dst->v.string_val)
					return OTLP_ERR_NOMEM;
			}
			break;
		case OTLP_ATTR_INT64:
			dst->type = OTLP_ATTR_INT64;
			dst->v.int64_val = src->v.int64_val;
			break;
		case OTLP_ATTR_DOUBLE:
			dst->type = OTLP_ATTR_DOUBLE;
			dst->v.double_val = src->v.double_val;
			break;
		case OTLP_ATTR_BOOL:
			dst->type = OTLP_ATTR_BOOL;
			dst->v.bool_val = src->v.bool_val;
			break;
		case OTLP_ATTR_BYTES:
			dst->type = OTLP_ATTR_BYTES;
			dst->v.bytes_val.len = src->v.bytes_val.len;
			if (src->v.bytes_val.len > 0)
			{
				dst->v.bytes_val.data =
					otlp_malloc(src->v.bytes_val.len);
				if (!dst->v.bytes_val.data)
					return OTLP_ERR_NOMEM;
				memcpy(dst->v.bytes_val.data,
					src->v.bytes_val.data,
					src->v.bytes_val.len);
			}
			break;
		case OTLP_ATTR_ARRAY:
		{
			const struct otlp_attr_array *arr = src->v.array_val;

			dst->type = OTLP_ATTR_ARRAY;
			if (!arr)
				break;
			dst->v.array_val = otlp_calloc(1, sizeof(*arr));
			if (!dst->v.array_val)
				return OTLP_ERR_NOMEM;
			if (arr->n > SIZE_MAX / sizeof(*arr->items))
			{
				otlp_free(dst->v.array_val);
				dst->v.array_val = NULL;
				return OTLP_ERR_NOMEM;
			}
			dst->v.array_val->items =
				otlp_calloc(arr->n, sizeof(*arr->items));
			if (!dst->v.array_val->items)
			{
				otlp_free(dst->v.array_val);
				dst->v.array_val = NULL;
				return OTLP_ERR_NOMEM;
			}
			dst->v.array_val->n = arr->n;
			for (i = 0; i < arr->n; i++)
				if (attr_copy_one(&dst->v.array_val->items[i],
					    &arr->items[i]) != OTLP_OK)
				{
					/* The failing item may be partially
					 * built; free is safe on partial
					 * state, then free the fully-built
					 * predecessors. */
					do
						otlp_attribute_free(
							&dst->v.array_val
								 ->items[i]);
					while (i-- > 0);
					otlp_free(dst->v.array_val->items);
					otlp_free(dst->v.array_val);
					dst->v.array_val = NULL;
					return OTLP_ERR_NOMEM;
				}
			break;
		}
		case OTLP_ATTR_KVLIST:
		{
			const struct otlp_attr_kvlist *kvl = src->v.kvlist_val;

			dst->type = OTLP_ATTR_KVLIST;
			if (!kvl)
				break;
			dst->v.kvlist_val = otlp_calloc(1, sizeof(*kvl));
			if (!dst->v.kvlist_val)
				return OTLP_ERR_NOMEM;
			if (kvl->n > SIZE_MAX / sizeof(*kvl->entries))
			{
				otlp_free(dst->v.kvlist_val);
				dst->v.kvlist_val = NULL;
				return OTLP_ERR_NOMEM;
			}
			dst->v.kvlist_val->entries =
				otlp_calloc(kvl->n, sizeof(*kvl->entries));
			if (!dst->v.kvlist_val->entries)
			{
				otlp_free(dst->v.kvlist_val);
				dst->v.kvlist_val = NULL;
				return OTLP_ERR_NOMEM;
			}
			dst->v.kvlist_val->n = kvl->n;
			for (i = 0; i < kvl->n; i++)
			{
				struct otlp_attr_kvlist_entry *e =
					&dst->v.kvlist_val->entries[i];

				if (kvl->entries[i].key)
				{
					e->key = otlp_dup_str(
						kvl->entries[i].key);
					if (!e->key)
						goto kvl_fail;
				}
				if (attr_copy_one(&e->value,
					    &kvl->entries[i].value) != OTLP_OK)
					goto kvl_fail;
				continue;
			kvl_fail:
				do
				{
					otlp_free(dst->v.kvlist_val->entries[i]
							  .key);
					otlp_attribute_free(
						&dst->v.kvlist_val->entries[i]
							 .value);
				} while (i-- > 0);
				otlp_free(dst->v.kvlist_val->entries);
				otlp_free(dst->v.kvlist_val);
				dst->v.kvlist_val = NULL;
				return OTLP_ERR_NOMEM;
			}
			break;
		}
		default:
			return OTLP_ERR_NOMEM;
	}
	return OTLP_OK;
}

otlp_status_t
otlp_attribute_copy_all(struct otlp_attribute *dst,
	const struct otlp_attribute *src,
	size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (attr_copy_one(&dst[i], &src[i]) != OTLP_OK)
			goto fail;
	return OTLP_OK;

fail:
	/* Free partial copies. The item at index i may be partially
	 * built; otlp_attribute_free is safe on partial state — it
	 * no-ops on NULL fields and recurses into built trees. */
	otlp_attribute_free(&dst[i]);
	while (i > 0)
	{
		i--;
		otlp_attribute_free(&dst[i]);
	}
	return OTLP_ERR_NOMEM;
}

/* ── ArrayValue / KeyValueList builders ───────────────────────── */

/* Fill one internal attribute from a public scalar value. */
static otlp_status_t
value_fill(struct otlp_attribute *a, const otlp_value_t *v)
{
	a->key = NULL;
	a->type = OTLP_ATTR_STRING;
	a->v.string_val = NULL;
	switch (v->type)
	{
		case OTLP_VALUE_STRING:
			if (!otlp_str_is_utf8(v->v.string_val))
				return OTLP_ERR_UTF8;
			a->type = OTLP_ATTR_STRING;
			a->v.string_val = otlp_dup_str(
				v->v.string_val ? v->v.string_val : "");
			return a->v.string_val ? OTLP_OK : OTLP_ERR_NOMEM;
		case OTLP_VALUE_BOOL:
			a->type = OTLP_ATTR_BOOL;
			a->v.bool_val = v->v.bool_val;
			return OTLP_OK;
		case OTLP_VALUE_INT64:
			a->type = OTLP_ATTR_INT64;
			a->v.int64_val = v->v.int64_val;
			return OTLP_OK;
		case OTLP_VALUE_DOUBLE:
			a->type = OTLP_ATTR_DOUBLE;
			a->v.double_val = v->v.double_val;
			return OTLP_OK;
		case OTLP_VALUE_BYTES:
			a->type = OTLP_ATTR_BYTES;
			a->v.bytes_val.len = v->v.bytes_val.len;
			if (v->v.bytes_val.len > 0)
			{
				a->v.bytes_val.data =
					otlp_dup_bytes(v->v.bytes_val.data,
						v->v.bytes_val.len);
				if (!a->v.bytes_val.data)
					return OTLP_ERR_NOMEM;
			}
			return OTLP_OK;
		default:
			return OTLP_ERR_INVALID_ARGUMENT;
	}
}

otlp_status_t
otlp_attr_array_build(const otlp_value_t *items,
	size_t n,
	struct otlp_attr_array **out)
{
	struct otlp_attr_array *arr;
	size_t i;

	*out = NULL;
	if (n > 0 && !items)
		return OTLP_ERR_NULL;
	if (n > SIZE_MAX / sizeof(*arr->items))
		return OTLP_ERR_INVALID_ARGUMENT;
	arr = otlp_calloc(1, sizeof(*arr));
	if (!arr)
		return OTLP_ERR_NOMEM;
	if (n > 0)
	{
		arr->items = otlp_calloc(n, sizeof(*arr->items));
		if (!arr->items)
		{
			otlp_free(arr);
			return OTLP_ERR_NOMEM;
		}
		arr->n = n;
	}
	for (i = 0; i < n; i++)
	{
		otlp_status_t st = value_fill(&arr->items[i], &items[i]);

		if (st != OTLP_OK)
		{
			do
				otlp_attribute_free(&arr->items[i]);
			while (i-- > 0);
			otlp_free(arr->items);
			otlp_free(arr);
			return st;
		}
	}
	*out = arr;
	return OTLP_OK;
}

otlp_status_t
otlp_attr_kvlist_build(const otlp_kv_t *entries,
	size_t n,
	struct otlp_attr_kvlist **out)
{
	struct otlp_attr_kvlist *kvl;
	size_t i;

	*out = NULL;
	if (n > 0 && !entries)
		return OTLP_ERR_NULL;
	if (n > SIZE_MAX / sizeof(*kvl->entries))
		return OTLP_ERR_INVALID_ARGUMENT;
	kvl = otlp_calloc(1, sizeof(*kvl));
	if (!kvl)
		return OTLP_ERR_NOMEM;
	if (n > 0)
	{
		kvl->entries = otlp_calloc(n, sizeof(*kvl->entries));
		if (!kvl->entries)
		{
			otlp_free(kvl);
			return OTLP_ERR_NOMEM;
		}
		kvl->n = n;
	}
	for (i = 0; i < n; i++)
	{
		otlp_status_t st;

		if (!entries[i].key)
			st = OTLP_ERR_NULL;
		else if (!otlp_str_is_utf8(entries[i].key))
			st = OTLP_ERR_UTF8;
		else
			st = value_fill(
				&kvl->entries[i].value, &entries[i].value);
		if (st != OTLP_OK)
		{
			do
			{
				otlp_free(kvl->entries[i].key);
				otlp_attribute_free(&kvl->entries[i].value);
			} while (i-- > 0);
			otlp_free(kvl->entries);
			otlp_free(kvl);
			return st;
		}
		kvl->entries[i].key = otlp_dup_str(entries[i].key);
		if (!kvl->entries[i].key)
		{
			do
			{
				otlp_free(kvl->entries[i].key);
				otlp_attribute_free(&kvl->entries[i].value);
			} while (i-- > 0);
			otlp_free(kvl->entries);
			otlp_free(kvl);
			return OTLP_ERR_NOMEM;
		}
	}
	*out = kvl;
	return OTLP_OK;
}

void
otlp_attr_array_free(struct otlp_attr_array *arr)
{
	size_t i;

	if (!arr)
		return;
	for (i = 0; i < arr->n; i++)
		otlp_attribute_free(&arr->items[i]);
	otlp_free(arr->items);
	otlp_free(arr);
}

void
otlp_attr_kvlist_free(struct otlp_attr_kvlist *kvl)
{
	size_t i;

	if (!kvl)
		return;
	for (i = 0; i < kvl->n; i++)
	{
		otlp_free(kvl->entries[i].key);
		otlp_attribute_free(&kvl->entries[i].value);
	}
	otlp_free(kvl->entries);
	otlp_free(kvl);
}

/* ── Lazy attribute lists ─────────────────────────────────────── */

bool
otlp_attr_list_find(const struct otlp_attribute *attrs,
	size_t n,
	const char *key,
	size_t *idx_out)
{
	size_t i;

	if (!attrs || !key)
		return false;
	for (i = 0; i < n; i++)
		if (attrs[i].key && strcmp(attrs[i].key, key) == 0)
		{
			if (idx_out)
				*idx_out = i;
			return true;
		}
	return false;
}

otlp_status_t
otlp_attr_vec_reserve(struct otlp_attr_vec *vec,
	size_t max,
	const char *key,
	struct otlp_attribute **out)
{
	struct otlp_attribute *slot;
	char *kc;
	size_t idx;

	if (!vec || !out || !key)
		return OTLP_ERR_NULL;
	/* Upsert: an existing key's slot is reused (old value
	 * released, count unchanged, position preserved). Overwrite
	 * succeeds even at max. */
	if (otlp_attr_list_find(vec->items, vec->n, key, &idx))
	{
		slot = &vec->items[idx];
		otlp_attribute_release_value(slot);
		*out = slot;
		return OTLP_OK;
	}
	if (vec->n >= max)
		return OTLP_ERR_OVERFLOW;
	/* Grow-on-demand: 4 slots initially, double on full, clamped
	 * to max. A realloc failure leaves the old array intact. */
	if (vec->n == vec->cap)
	{
		size_t new_cap = vec->cap ? vec->cap * 2 : (4 < max ? 4 : max);
		struct otlp_attribute *grown;

		if (new_cap > max)
			new_cap = max;
		grown = otlp_realloc(vec->items, new_cap * sizeof(*grown));
		if (!grown)
			return OTLP_ERR_NOMEM;
		memset(grown + vec->cap,
			0,
			(new_cap - vec->cap) * sizeof(*grown));
		vec->items = grown;
		vec->cap = new_cap;
	}
	if (!otlp_str_is_utf8(key))
		return OTLP_ERR_UTF8;
	kc = otlp_dup_str(key);
	if (!kc)
		return OTLP_ERR_NOMEM;
	slot = &vec->items[vec->n];
	slot->key = kc;
	/* Zero the union so cleanup paths don't see garbage. */
	slot->v.string_val = NULL;
	slot->type = OTLP_ATTR_STRING;
	vec->n++;
	*out = slot;
	return OTLP_OK;
}

bool
otlp_str_is_utf8(const char *s)
{
	const unsigned char *p = (const unsigned char *) s;

	if (!s)
		return true;
	while (*p)
	{
		unsigned char c = *p;
		size_t need;
		uint32_t cp, cp_min;

		if (c < 0x80)
		{
			p++;
			continue;
		}
		else if ((c & 0xe0) == 0xc0)
		{
			need = 1;
			cp = c & 0x1f;
			cp_min = 0x80;
		}
		else if ((c & 0xf0) == 0xe0)
		{
			need = 2;
			cp = c & 0x0f;
			cp_min = 0x800;
		}
		else if ((c & 0xf8) == 0xf0)
		{
			need = 3;
			cp = c & 0x07;
			cp_min = 0x10000;
		}
		else
			return false; /* stray continuation or 0xf8+ lead */
		for (size_t i = 0; i < need; i++)
		{
			unsigned char cc = p[1 + i];

			/* Also stops at the NUL terminator: a truncated
			 * sequence can never pass. */
			if ((cc & 0xc0) != 0x80)
				return false;
			cp = (cp << 6) | (cc & 0x3f);
		}
		if (cp < cp_min) /* overlong encoding */
			return false;
		if (cp >= 0xd800 && cp <= 0xdfff) /* UTF-16 surrogate */
			return false;
		if (cp > 0x10ffff)
			return false;
		p += need + 1;
	}
	return true;
}

/* ── The set-attribute engine ─────────────────────────────────── */

otlp_status_t
otlp_attr_vec_set(struct otlp_attr_vec *vec,
	size_t max,
	const char *key,
	const otlp_value_t *v)
{
	struct otlp_attribute *a;
	char *s_copy = NULL;
	uint8_t *b_copy = NULL;
	size_t b_len = 0;
	otlp_status_t st;

	if (!vec || !key || !v)
		return OTLP_ERR_NULL;
	/* Owned payloads are duplicated BEFORE the reserve: the slot
	 * is committed by reserve, so the fill must not fail. */
	switch (v->type)
	{
		case OTLP_VALUE_STRING:
			if (!otlp_str_is_utf8(v->v.string_val))
				return OTLP_ERR_UTF8;
			s_copy = otlp_dup_str(
				v->v.string_val ? v->v.string_val : "");
			if (!s_copy)
				return OTLP_ERR_NOMEM;
			break;
		case OTLP_VALUE_BYTES:
			b_len = v->v.bytes_val.len;
			if (b_len > 0 && !v->v.bytes_val.data)
				return OTLP_ERR_NULL;
			if (b_len > 0)
			{
				b_copy = otlp_dup_bytes(
					v->v.bytes_val.data, b_len);
				if (!b_copy)
					return OTLP_ERR_NOMEM;
			}
			break;
		case OTLP_VALUE_BOOL:
		case OTLP_VALUE_INT64:
		case OTLP_VALUE_DOUBLE:
			break;
		default:
			return OTLP_ERR_INVALID_ARGUMENT;
	}
	st = otlp_attr_vec_reserve(vec, max, key, &a);
	if (st != OTLP_OK)
	{
		otlp_free(s_copy);
		otlp_free(b_copy);
		return st;
	}
	switch (v->type)
	{
		case OTLP_VALUE_STRING:
			a->type = OTLP_ATTR_STRING;
			a->v.string_val = s_copy;
			break;
		case OTLP_VALUE_BYTES:
			a->type = OTLP_ATTR_BYTES;
			a->v.bytes_val.data = b_copy;
			a->v.bytes_val.len = b_len;
			break;
		case OTLP_VALUE_BOOL:
			a->type = OTLP_ATTR_BOOL;
			a->v.bool_val = v->v.bool_val;
			break;
		case OTLP_VALUE_INT64:
			a->type = OTLP_ATTR_INT64;
			a->v.int64_val = v->v.int64_val;
			break;
		default:
			a->type = OTLP_ATTR_DOUBLE;
			a->v.double_val = v->v.double_val;
			break;
	}
	return OTLP_OK;
}

otlp_status_t
otlp_attr_vec_set_array(struct otlp_attr_vec *vec,
	size_t max,
	const char *key,
	const otlp_value_t *items,
	size_t n)
{
	struct otlp_attribute *a;
	struct otlp_attr_array *arr;
	otlp_status_t st;

	if (!vec || !key)
		return OTLP_ERR_NULL;
	st = otlp_attr_array_build(items, n, &arr);
	if (st != OTLP_OK)
		return st;
	st = otlp_attr_vec_reserve(vec, max, key, &a);
	if (st != OTLP_OK)
	{
		otlp_attr_array_free(arr);
		return st;
	}
	a->type = OTLP_ATTR_ARRAY;
	a->v.array_val = arr;
	return OTLP_OK;
}

otlp_status_t
otlp_attr_vec_set_kvlist(struct otlp_attr_vec *vec,
	size_t max,
	const char *key,
	const otlp_kv_t *entries,
	size_t n)
{
	struct otlp_attribute *a;
	struct otlp_attr_kvlist *kvl;
	otlp_status_t st;

	if (!vec || !key)
		return OTLP_ERR_NULL;
	st = otlp_attr_kvlist_build(entries, n, &kvl);
	if (st != OTLP_OK)
		return st;
	st = otlp_attr_vec_reserve(vec, max, key, &a);
	if (st != OTLP_OK)
	{
		otlp_attr_kvlist_free(kvl);
		return st;
	}
	a->type = OTLP_ATTR_KVLIST;
	a->v.kvlist_val = kvl;
	return OTLP_OK;
}

otlp_status_t
otlp_attr_vec_copy(struct otlp_attr_vec *dst, const struct otlp_attr_vec *src)
{
	if (!dst || !src)
		return OTLP_ERR_NULL;
	if (src->n == 0)
		return OTLP_OK;
	if (src->n > SIZE_MAX / sizeof(*src->items))
		return OTLP_ERR_NOMEM;
	dst->items = otlp_calloc(src->n, sizeof(*dst->items));
	if (!dst->items)
		return OTLP_ERR_NOMEM;
	if (otlp_attribute_copy_all(dst->items, src->items, src->n) != OTLP_OK)
	{
		otlp_free(dst->items);
		dst->items = NULL;
		return OTLP_ERR_NOMEM;
	}
	dst->cap = src->n;
	dst->n = src->n;
	return OTLP_OK;
}

void
otlp_attr_vec_free(struct otlp_attr_vec *vec)
{
	size_t i;

	if (!vec || !vec->items)
	{
		if (vec)
			vec->n = 0;
		return;
	}
	for (i = 0; i < vec->n; i++)
		otlp_attribute_free(&vec->items[i]);
	otlp_free(vec->items);
	vec->items = NULL;
	vec->n = 0;
	vec->cap = 0;
}


/* ── ID validation ────────────────────────────────────────────── */

bool
otlp_id_is_all_zero(const uint8_t *id, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		if (id[i])
			return false;
	return true;
}
