/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACED_DRIFT_HITS_H_
#define RETRACED_DRIFT_HITS_H_

/*
 * Live hit-level drift grading (TODO.impl/18): the matcher that
 * names WHAT escaped, beside the counts retrace.drift.summary
 * already reports.
 *
 * A libc CLAIM is the libc lane's own evidence (an event the
 * in-process agent emitted: it saw the call). A kernel
 * OBSERVATION is the kernel lane's sight of the same activity.
 * An observation that no claim covers -- same pid (when both
 * lanes carry one; <=0 is a wildcard), same op, same path,
 * within the window -- is a HIT: a sub-libc escape, named.
 *
 * Pure semantics, no transport/parson dependencies: the caller
 * (daemon_frame, the one EVENT site both transports share)
 * extracts the key and owns the journaling of the hits this
 * module names. Locking: the caller's transport lock covers
 * every call.
 */

/* Record a libc-layer claim. pid <= 0 = lane carries no pid. */
void retrace_drift_claim(long pid, const char *op,
	const char *path);

/*
 * Grade a kernel observation: 1 when no covering claim exists
 * (a named sub-libc escape, NOT deduped away), 0 otherwise.
 * A pathless observation is never a hit -- it names nothing
 * actionable; the count already rides kernel_obs.
 */
int retrace_drift_observe(long pid, const char *op,
	const char *path);

/* Test/probe seam: forget every claim and hit. */
void retrace_drift_hits_reset(void);

#endif /* RETRACED_DRIFT_HITS_H_ */
