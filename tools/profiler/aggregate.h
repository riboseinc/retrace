/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_PROFILER_AGGREGATE_H_
#define RETRACE_PROFILER_AGGREGATE_H_

#include "parson.h"

#include <stddef.h>

/*
 * Profile aggregation (TODO.windows/08): reduce a retrace trace
 * (any layer: libc capture, kernel truth via ptrace/eBPF/
 * procmon2retrace) to what a binary DOES -- functions called,
 * filesystem accesses by class, env vars read, network
 * addresses. Pure logic; the CLI and the risk report sit on
 * top.
 *
 * Paths are normalized with the correlate normalizer (match.h)
 * so NT forms and mixed slashes meet.
 */

/* A growable name->count set, kept sorted by name (bsearch). */
struct ProfNames {
	char **names;
	size_t *counts;
	size_t count;
	size_t cap;
};

void prof_names_init(struct ProfNames *n);
void prof_names_add(struct ProfNames *n, const char *name);
size_t prof_names_get(const struct ProfNames *n, const char *name);
void prof_names_free(struct ProfNames *n);

/*
 * One filesystem access (aggregated): the normalized path, its
 * most severe class seen (write > read > probe), and the number
 * of observations.
 */
struct ProfAccess {
	char *path;
	int class_write;   /* any write seen */
	int class_read;    /* any read seen */
	int class_probe;   /* any probe seen */
	size_t hits;
};

struct ProfAccesses {
	struct ProfAccess *items;
	size_t count;
	size_t cap;
};

/*
 * Per-function call timings (TODO.trace-profile/21): harvested
 * from call_duration_us return summaries. Samples are kept
 * (bounded) so percentiles are real, not approximated.
 */
#define PROF_TIMING_SAMPLES_MAX 4096

struct ProfTiming {
	char *func;
	size_t calls;
	double total_us;
	double max_us;
	double *samples;   /* bounded; percentile input */
	size_t sample_cnt;
	size_t sample_cap;
};

struct ProfTimings {
	struct ProfTiming *items;  /* sorted by func (bsearch add) */
	size_t count;
	size_t cap;
};

/*
 * The aggregate profile of one trace.
 */
struct Profile {
	struct ProfNames functions;   /* func name -> call count */
	struct ProfNames env;         /* env var names read */
	struct ProfNames net;         /* addresses contacted (host:port) */
	struct ProfAccesses accesses;
	struct ProfTimings timings;   /* per-func call durations */
	size_t entries;               /* entries consumed */
	size_t no_pid;                /* entries without pid identity */
};

void prof_init(struct Profile *p);

/* p99 (or max when fewer than 100 samples) for one timing. */
double prof_timing_p99(const struct ProfTiming *t);
void prof_add_entry(JSON_Object *entry, struct Profile *p);
/* Sort indexes for lookup; required before contains/get. */
void prof_finish(struct Profile *p);
void prof_free(struct Profile *p);

/*
 * Look up an access by normalized path. Requires finish().
 * Returns NULL when absent.
 */
struct ProfAccess *prof_access_get(struct Profile *p, const char *path);

/* Serialize to an owned JSON object (caller frees). */
JSON_Value *prof_to_json(const struct Profile *p);

/*
 * prof_to_json's inverse: fill p from a profile doc's "profile"
 * object. Returns 0 on success, -1 when root is NULL. The
 * profile.json reader for the jail subcommand.
 */
int prof_from_json(const JSON_Object *root, struct Profile *p);

#endif /* RETRACE_PROFILER_AGGREGATE_H_ */
