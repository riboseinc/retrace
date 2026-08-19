/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_CORRELATE_MATCH_H_
#define RETRACE_CORRELATE_MATCH_H_

#include "parson.h"

#include <stddef.h>

/*
 * Escape-correlation matching (TODO.next-level/02, tebako).
 *
 * An "escape" is an outside-stream (retrace) event touching a
 * path under the virtualized prefix that the inside stream (the
 * VFS, e.g. tebako's tfs) never saw. Pure logic: path
 * normalization, path extraction from JSON entries, and the
 * sorted path-set used for the inside ∩ ¬inside decision.
 */

#define CORR_PATH_MAX 1024

/*
 * Normalize a path into out (always NUL-terminated):
 *   - strip NT prefixes "\??\" and "\\?\" and rewrite the
 *     "\Device\HarddiskVolumeN\" volume form to a DOS drive
 * guess (N -> 'A'+N-1, so volume 3 -> C: -- on a standard
 * install the first visible volume is 3; documented
 * heuristic, overridden by normalizing both sides of a
 * join through this same function)
 *   - backslashes become forward slashes (matching is
 * slash-agnostic)
 *   - trailing slash stripped (unless it is the whole path)
 *   - POSIX paths stay case-sensitive; drive-letter prefixes are
 * compared case-insensitively (see corr_pathcmp)
 * Returns the number of bytes written including the terminating
 * NUL, or 0 if the input is not path-like (see corr_is_path_like)
 * or does not fit.
 */
size_t corr_normalize(const char *in, char *out, size_t outsz);

/*
 * 1 if the string looks like a path: contains a slash and starts
 * with '/', a drive letter + ':', '\', or contains '/'.
 */
int corr_is_path_like(const char *s);

/*
 * Comparison for normalized paths: case-sensitive except the
 * leading drive letter ("c:/x" == "C:/x"). Returns <0/0/>0 like
 * strcmp.
 */
int corr_pathcmp(const char *a, const char *b);

/* A growable, sorted set of owned normalized-path strings. */
struct CorrSet {
	char **items;
	size_t count;
	size_t cap;
};

void corr_set_init(struct CorrSet *s);

/* Insert an owned copy (deduplicated). Returns 0/-1. */
int corr_set_add(struct CorrSet *s, const char *path);

/* Sort after all inserts (makes contains a bsearch). */
void corr_set_finish(struct CorrSet *s);

/* 1 if the normalized path is present. Requires finish(). */
int corr_set_contains(const struct CorrSet *s, const char *path);

void corr_set_free(struct CorrSet *s);

/*
 * Extract every path-like string value from a JSON entry (any
 * depth: top-level fields plus everything under "message") and
 * add its NORMALIZED form to the set. Returns the number of
 * paths added (0 if none).
 */
int corr_collect_paths(JSON_Object *entry, struct CorrSet *out);

/*
 * One escape hit, for reporting.
 */
struct CorrEscape {
	char	    path[CORR_PATH_MAX];
	const char *func; /* borrowed from the entry, may be NULL */
	long	    tid;  /* entry "tid" field, 0 if absent */
};

/*
 * Walk one outside entry; if it carries a path under `prefix`
 * (normalized comparison) that is NOT in `inside`, fill *out
 * with the first such hit and return 1. Return 0 if the entry is
 * clean (no prefix path, or fully covered). Requires
 * corr_set_finish(inside) beforehand.
 */
int corr_entry_is_escape(JSON_Object *entry,
			 const char *prefix,
			 const struct CorrSet *inside,
			 struct CorrEscape *out);

#endif /* RETRACE_CORRELATE_MATCH_H_ */
