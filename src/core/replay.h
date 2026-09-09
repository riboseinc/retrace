/*
 * Copyright (c) 2017, [Ribose Inc](https://ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_CORE_REPLAY_H_
#define RETRACE_CORE_REPLAY_H_

/*
 * The replay slice (TODO.impl/03): a recorded session replays
 * with identical synthesized outcomes -- deterministically,
 * across machines, without the original time seed. The 3am
 * failure reproduces at 9am on the axes retrace controls.
 *
 * Modes (env, mutually exclusive, lazily resolved on first
 * synthesized outcome -- no boot-order coupling):
 *   RETRACE_REPLAY_OUT=FILE  record: persist the resolved fuzz
 *                            seed, then every synthesized
 *                            outcome (one line each)
 *   RETRACE_REPLAY_IN=FILE   replay: force the recorded seed
 *                            back into the fuzz machinery and
 *                            compare every synthesized outcome
 *                            against the record in order
 *
 * The seams are exactly the random ones (MECE): memory_fuzz's
 * fail decision and fuzz_str's dictionary pick. Deterministic
 * actions (fail_first, incomplete_io, modify_*) need no record
 * -- their config already is one.
 *
 * Single-threaded targets (the classic record/replay
 * constraint): the underlying rand() stream is not thread-
 * ordered; the module adds no locks around it.
 */

/* 0 none, 1 record, 2 replay */
int retrace_replay_mode(void);

/*
 * Seed seam: callers pass the seed they resolved; record
 * persists it (first line) and returns it unchanged, replay
 * returns the RECORDED seed -- the time fallback never
 * re-rolls. Call once per run (fuzz_seed_init).
 */
unsigned int retrace_replay_seed(unsigned int chosen);

/*
 * Outcome seam: record appends {func,kind,value}; replay
 * compares against the next record line in order and counts
 * divergence (exhaustion included). value is the synthesized
 * token/decision -- strings, so every seam speaks one type.
 */
void retrace_replay_note(const char *func, const char *kind,
	const char *value);

/* deinit: the verdict -- matched/diverged lines in the log */
void retrace_replay_report(void);

#endif /* RETRACE_CORE_REPLAY_H_ */
