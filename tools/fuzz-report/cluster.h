/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_TOOLS_FUZZ_REPORT_CLUSTER_H_
#define RETRACE_TOOLS_FUZZ_REPORT_CLUSTER_H_

#include "parson.h"

#include <stddef.h>
#include <stdio.h>

/*
 * Crash clustering (TODO.trace-profile/20). Pure logic over
 * traces: an iteration's signature is (last-called function,
 * param count) -- the last func entry before death names where
 * the target died. Iterations with the same signature are the
 * same bug (v1 heuristic, documented).
 */

struct FuzzCluster {
	unsigned long id;       /* signature hash */
	char func[64];          /* last-called function */
	int params;             /* its param count */
	size_t count;           /* iterations in this cluster */
	unsigned long first_seed; /* reproducer seed */
	int is_crash;           /* 1: signal death; 0: assertion */
};

struct FuzzReport {
	struct FuzzCluster *clusters;
	size_t count;
	size_t cap;
	size_t total;           /* iterations run */
	size_t crashes;         /* signal deaths */
	size_t assertions;      /* marker hits */
};

void fuzz_report_init(struct FuzzReport *r);
void fuzz_report_free(struct FuzzReport *r);

/*
 * Fold one iteration into the report. exit_status: the raw
 * waitpid status (signal deaths extracted here). trace_json:
 * the iteration's trace text (may be NULL/empty). seed: the
 * RETRACE_FUZZ_SEED value used. marker: assertion substring
 * found in the trace (NULL disables assertion classification).
 * Returns the cluster id the iteration landed in.
 */
unsigned long fuzz_report_fold(struct FuzzReport *r, int exit_status,
	const char *trace_json, unsigned long seed, const char *marker);

/* Serialize (owned JSON object; caller frees). */
JSON_Value *fuzz_report_to_json(const struct FuzzReport *r);

#endif /* RETRACE_TOOLS_FUZZ_REPORT_CLUSTER_H_ */
