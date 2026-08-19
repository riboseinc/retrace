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
 * Escape-correlation matching (TODO.windows/01-03, tebako).
 *
 * An "escape" is an outside-stream (retrace) event touching a
 * path under the virtualized prefix that the inside stream (the
 * VFS, e.g. tebako's tfs) never saw. The JSON entry is the SSOT
 * for the decision inputs: pid, tid, time live on the entry;
 * paths live in any string field at any depth; func/detail in
 * the message drive classification.
 */

#define CORR_PATH_MAX 1024

/*
 * Normalize a path into out (always NUL-terminated):
 *   - strip NT prefixes "\??\", "\\?\" and the '//?/' spelling
 *     (libsass builds it before flipping separators), and
 *     rewrite the "\Device\HarddiskVolumeN\" volume form to a
 *     DOS drive guess (N -> 'A'+N-1, so volume 3 -> C: -- on a
 *     standard install the first visible volume is 3; documented
 *     heuristic, overridden by normalizing both sides of a
 *     join through this same function)
 *   - backslashes become forward slashes (matching is
 *     slash-agnostic)
 *   - trailing slash stripped (unless it is the whole path)
 *   - POSIX paths stay case-sensitive; drive-letter prefixes are
 *     compared case-insensitively (see corr_pathcmp)
 * Returns the number of bytes written including the terminating
 * NUL, or 0 if the input is not path-like (see corr_is_path_like)
 * or does not fit.
 */
size_t corr_normalize(const char *in, char *out, size_t outsz);

/* 1 if the string looks like a path: contains a slash and starts
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
 * Event classification (TODO.windows/03). PROBE = existence
 * leak (read-attributes semantics: QueryOpen, stat, access);
 * WRITE = potential mutation; READ = data access; NONE = no
 * func on the entry (e.g. retrace ACT entries).
 */
enum CorrClass {
	CORR_CLS_NONE = 0,
	CORR_CLS_PROBE,
	CORR_CLS_READ,
	CORR_CLS_WRITE
};

/*
 * Classify one event by its func name and Detail/params text.
 * Case-insensitive. Heuristics, documented and pinned by unit
 * tests: the probe set is name-keyed; CreateFile/NtCreateFile
 * with "Write" in detail classifies WRITE; the write set is
 * name-keyed; everything else with a func is READ.
 */
enum CorrClass corr_classify(const char *func, const char *detail);

const char *corr_class_str(enum CorrClass cls);

/*
 * One observed path event (inside stream record).
 */
struct CorrRec {
	char *path;  /* owned, normalized */
	long pid;    /* entry pid, 0 if absent */
	double time; /* entry time, 0 if absent */
};

/*
 * The inside index: records sorted by (path, time) plus the
 * sorted unique-path set for the fast "never seen" reject.
 */
struct CorrIndex {
	struct CorrRec *recs;
	size_t count;
	size_t cap;
	struct CorrSet set;
};

void corr_index_init(struct CorrIndex *idx);

/*
 * Extract every path-like string value from the entry (any
 * depth) and add one record per path carrying the entry's pid
 * and time.
 */
void corr_index_add_entry(JSON_Object *entry, struct CorrIndex *idx);

/* Sort records and the set. Required before is-escape queries. */
void corr_index_finish(struct CorrIndex *idx);

void corr_index_free(struct CorrIndex *idx);

/*
 * One escape hit, for reporting.
 */
struct CorrEscape {
	char path[CORR_PATH_MAX];
	const char *func; /* borrowed from the entry, may be NULL */
	long tid;         /* entry "tid" field, 0 if absent */
	long pid;         /* entry "pid" field, 0 if absent */
	double time;      /* entry "time" field, 0 if absent */
	enum CorrClass cls;
};

/*
 * The decision criteria value object: new criteria extend this
 * struct (OCP), never fork the code path.
 */
struct CorrCriteria {
	const char *prefix; /* normalized virtualization root */
	long pid;           /* 0 = every pid (TODO.windows/01) */
	double window;      /* seconds; 0 = pure set-difference
			     * (TODO.windows/02: lazy materialize)
			     */
	int exclude_probes; /* drop probe-class hits from the
			     * report (jail-grant policy)
			     */
};

/*
 * Walk one outside entry; if it carries a path under
 * criteria->prefix that is NOT covered by the inside index,
 * fill *out with the first such hit and return 1. Return 0 if
 * the entry is clean.
 *
 * Coverage (TODO.windows/01-02): the path was seen by the
 * inside stream from the SAME pid (pid-less entries are
 * wildcards), within criteria->window seconds when a window is
 * set.
 *
 * Requires corr_index_finish(inside) beforehand.
 */
int corr_entry_is_escape(JSON_Object *entry,
			 const struct CorrCriteria *criteria,
			 const struct CorrIndex *inside,
			 struct CorrEscape *out);

#endif /* RETRACE_CORRELATE_MATCH_H_ */
