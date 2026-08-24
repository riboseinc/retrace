/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Internal utilities shared across src/ files. NOT part of the
 * public API; do not include from include/.
 */
#ifndef OTLP_C_INTERNAL_UTIL_H
#define OTLP_C_INTERNAL_UTIL_H

#include <otlp-c/status.h>
#include <otlp-c/value.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Forward declaration — the full struct is in span_internal.h.
 * We avoid including span_internal.h here to prevent a dependency
 * cycle (span.c → internal_util.h → span_internal.h). Only the
 * .c file needs the full definition. */
struct otlp_attribute;
struct otlp_attr_array;
struct otlp_attr_kvlist;
struct otlp_attr_vec;

/* ── Custom-allocator-backed wrappers ───────────────────────────
 *
 * All source .c files under src/ MUST use these instead of
 * malloc/free/realloc/calloc directly. The wrappers dispatch
 * through the global allocator set via otlp_set_allocator()
 * (see include/otlp-c/allocator.h).
 */
void *
otlp_malloc(size_t n);
void *
otlp_realloc(void *p, size_t n);
void
otlp_free(void *p);
void *
otlp_calloc(size_t count, size_t size);

/* ── String / byte duplication ────────────────────────────────── */

/* Heap-allocate a copy of `s` (NUL-terminated). NULL on OOM.
 * NULL input returns NULL. */
char *
otlp_dup_str(const char *s);

/* True iff `s` is valid UTF-8 (protobuf proto3 `string` contract).
 * NULL counts as valid (treated as absent). OTLP string fields MUST
 * be valid UTF-8 — Go-based collectors (otelcol) reject the whole
 * ExportRequest on unmarshal otherwise, so the library validates at
 * the API boundary and fails the setter instead (OTLP_ERR_UTF8). */
bool
otlp_str_is_utf8(const char *s);

/* Heap-allocate a copy of `len` bytes from `src`. NULL on OOM or
 * when len > 0 && src == NULL. */
uint8_t *
otlp_dup_bytes(const uint8_t *src, size_t len);

/* ── Attribute copy (shared by span/metric/log clones) ────────── */

/* Deep-copy `n` attributes from `src` to `dst`. dst must have at
 * least `n` slots. On failure, partial copies are freed. Returns
 * OTLP_OK or OTLP_ERR_NOMEM. Used by otlp_attr_vec_copy (the
 * shared clone path for every attribute-bearing object). */
otlp_status_t
otlp_attribute_copy_all(struct otlp_attribute *dst,
	const struct otlp_attribute *src,
	size_t n);

/* Release the attribute's union payload (string, bytes, or a
 * nested array/kvlist tree) and reset the type to a safe empty
 * value. The key is kept — this is the primitive for
 * replace-value-in-place (attribute upsert). */
void
otlp_attribute_release_value(struct otlp_attribute *a);

/* ── Attribute vectors (grow-on-demand) ─────────────────────────
 *
 * The one storage model for attribute arrays on spans, span
 * events/links, metrics, and log records (struct otlp_attr_vec,
 * span_internal.h): items is NULL until the first attribute and
 * the array grows 4 → 8 → … slots bounded by the owner's max — an
 * object with a handful of attributes pays for a handful of
 * slots, not a cap-sized array (v0.5.68/69 made them lazy;
 * v0.5.75 made them grow).
 *
 * Upsert semantics (v0.5.73): attributes are a map — the OTLP
 * data model requires unique keys and the OTel API defines
 * setting an attribute as last-write-wins. Reserve finds an
 * existing key and reuses its slot (releasing the old value) or
 * appends a new one; duplicates can no longer reach the wire.
 */

/* Linear search for `key` over `n` items. Returns true and writes
 * the index to *idx_out when found. */
bool
otlp_attr_list_find(const struct otlp_attribute *attrs,
	size_t n,
	const char *key,
	size_t *idx_out);

/* Reserve the slot for `key` in `vec` (bounded by `max`) and
 * commit the count: if the key already exists, its old value is
 * released and that slot is returned (count unchanged, position
 * preserved); otherwise the array grows if full and a slot is
 * appended (n incremented). Overwriting an existing key succeeds
 * even at max; appending past max returns OTLP_ERR_OVERFLOW.
 *
 * The caller fills type + value AFTER this returns. Because the
 * slot is already committed, the fill must not fail — duplicate
 * owned values (strings, bytes, composite trees) BEFORE calling
 * and free them if reserve itself fails. Scalar types have no
 * failure path. */
otlp_status_t
otlp_attr_vec_reserve(struct otlp_attr_vec *vec,
	size_t max,
	const char *key,
	struct otlp_attribute **out);

/* ── The set-attribute engine ───────────────────────────────────
 *
 * Every public *_set_attribute_* setter on every surface (span,
 * event, link, metric, log record) is a thin typed wrapper over
 * these three entry points — one owner for the whole flow (null
 * guards, value pre-duplication, upsert reserve, non-failing
 * fill). DRY: a guard or OOM bug can exist in one place, not
 * twenty-nine. OCP: a new value type extends the engine, and the
 * typed wrappers stay two-liners.
 */

/* Set `key` to the scalar `v` (upsert; see reserve). The value is
 * copied — string/bytes payloads are duplicated before the slot
 * is committed, so the fill cannot fail. */
otlp_status_t
otlp_attr_vec_set(struct otlp_attr_vec *vec,
	size_t max,
	const char *key,
	const otlp_value_t *v);

/* Set `key` to an ArrayValue built from `n` scalar values
 * (deep-copied). Same upsert semantics. */
otlp_status_t
otlp_attr_vec_set_array(struct otlp_attr_vec *vec,
	size_t max,
	const char *key,
	const otlp_value_t *items,
	size_t n);

/* Set `key` to a KeyValueList built from `n` entries
 * (deep-copied). Same upsert semantics. */
otlp_status_t
otlp_attr_vec_set_kvlist(struct otlp_attr_vec *vec,
	size_t max,
	const char *key,
	const otlp_kv_t *entries,
	size_t n);

/* Deep-copy `src` into `dst` as an exact-fit vector (n slots, no
 * spare capacity — a later append grows it). On success dst owns
 * the copy. On failure everything is freed and dst is empty. */
otlp_status_t
otlp_attr_vec_copy(struct otlp_attr_vec *dst, const struct otlp_attr_vec *src);

/* Free every attribute and the array itself; resets the vector to
 * empty. Safe on an already-empty vector. */
void
otlp_attr_vec_free(struct otlp_attr_vec *vec);

/* ── ArrayValue / KeyValueList trees ────────────────────────────
 *
 * Builders for the composite attribute types: they consume the
 * public flat inputs (arrays of scalar otlp_value_t) and return
 * fully-owned internal trees. Build FIRST, then reserve the
 * attribute slot, then attach — the reserve-and-fill contract
 * requires the fill to be non-failing assignments.
 */

/* Deep-build an ArrayValue tree from `n` scalar values. The
 * result is owned by the caller until attached to an attribute
 * slot; free with otlp_attr_array_free if never attached. */
otlp_status_t
otlp_attr_array_build(const otlp_value_t *items,
	size_t n,
	struct otlp_attr_array **out);

/* Deep-build a KeyValueList tree from `n` (key, value) entries.
 * Duplicate entry keys are kept as given — the uniqueness
 * contract applies to attribute keys, not list contents. */
otlp_status_t
otlp_attr_kvlist_build(const otlp_kv_t *entries,
	size_t n,
	struct otlp_attr_kvlist **out);

void
otlp_attr_array_free(struct otlp_attr_array *arr);
void
otlp_attr_kvlist_free(struct otlp_attr_kvlist *kvl);

/* ── ID validation ────────────────────────────────────────────── */

/* Returns true if all `len` bytes of `id` are zero. W3C Trace
 * Context §3.1.1/§3.1.2 forbids all-zero trace-id and parent-id;
 * callers must reject all-zero at set time so invalid IDs don't
 * reach the wire (where receivers reject them). */
bool
otlp_id_is_all_zero(const uint8_t *id, size_t len);

#endif
