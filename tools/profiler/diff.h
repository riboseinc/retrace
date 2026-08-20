/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_TOOLS_PROFILER_DIFF_H_
#define RETRACE_TOOLS_PROFILER_DIFF_H_

#include "aggregate.h"

#include "parson.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Profile drift (TODO.trace-profile/02): what does the candidate
 * build do that the baseline did not? The upgrade story for the
 * tailor loop: profile old -> upgrade -> profile new -> diff ->
 * update the jail.
 */

struct ProfPathChange {
	char *path;
	int class_from;		/* enum CorrClass, -1 when added */
	int class_to;		/* -1 when removed */
	size_t hits_from;
	size_t hits_to;
};

struct ProfDiff {
	/*
	 * path changes: added (from == -1), removed (to == -1),
	 * class-changed otherwise; sorted by path
	 */
	struct ProfPathChange *changes;
	size_t count;
	size_t cap;

	/* functions present only in the candidate */
	char **new_functions;
	size_t new_functions_cnt;
	size_t new_functions_cap;
};

void prof_diff_init(struct ProfDiff *d);

/*
 * Compute baseline -> candidate drift. Both profiles must be
 * finished (prof_finish). Returns 1 when any drift exists.
 */
int prof_diff_compute(const struct Profile *baseline,
		      const struct Profile *candidate,
		      struct ProfDiff *d);

JSON_Value *prof_diff_to_json(const struct ProfDiff *d);

void prof_diff_free(struct ProfDiff *d);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_TOOLS_PROFILER_DIFF_H_ */
