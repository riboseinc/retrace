/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */
#ifndef RETRACE_TOOLS_JOURNAL_H_
#define RETRACE_TOOLS_JOURNAL_H_

#include <stdint.h>
#include <stdio.h>

/*
 * The retraced append-only event journal (TODO.supervisor/02).
 *
 * The audit trail: every accepted EVENT lands as one JSONL
 * line, preceded by a hash-chain link -- tampering with any
 * historical line breaks every later chain verification
 * (plan 09's audit story). The chain is FNV-1a over
 * (prev_hash, line) -- NOT cryptographic (that arrives with
 * plan 05's signatures); it makes accidental corruption and
 * casual edits detectable, and it is cheap on the hot path.
 *
 * Replay (boot): read back the journal, re-verify the chain,
 * stop at the first torn tail line (crash-only state; losing
 * the trailing partial record is the contract). The registry
 * rebuilds agent liveness from the last HELLO/BYE per id.
 *
 * Durability contract (v2.45): control-plane records (auth,
 * policy, session, journal) are flushed on write; routine
 * telemetry is buffered and may lose the unflushed tail on an
 * unclean shutdown. The next boot journals
 * retrace.journal.unclean -- gaps are recorded, never silent.
 */

#include "registry.h"

struct retraced_journal {
	char path[512];		/* the LIVE segment's path */
	uint64_t prev_hash;
	uint64_t lines;
	/* replay statistics (plan 02's tests assert these) */
	uint64_t replay_ok;
	uint64_t replay_events;
	int chain_broken_at; /* line no. of first mismatch, -1 ok */
	/* writer state (the open-once deepening): the FILE* lives
	 * for the daemon's lifetime; routine telemetry is buffered
	 * by stdio and flushed at durability points + close
	 */
	FILE *f;
	int clean_close;

	/*
	 * Rotation (TODO.impl/10): the base path names the SERIES;
	 * segments are `<base>.<NNNN>` numbered from 0. The chain
	 * CONTINUES across segments -- each new segment's genesis
	 * link carries the predecessor's final head -- so the
	 * whole history stays one verifiable chain split across
	 * files. Retention prunes OLDEST segments only, and each
	 * prune is itself a chained record in the live segment:
	 * gaps are auditable, never silent. Rotation unset =
	 * exactly the single-file journal of old.
	 */
	char base[512];
	uint64_t rotate_bytes;	/* 0 = no size trigger */
	long rotate_seconds;	/* 0 = no time trigger */
	uint64_t budget_bytes;	/* 0 = unlimited retention */
	uint64_t seg_bytes;	/* the live segment's appends */
	long last_rotate_ts;
	int segment_seq;	/* the live segment's number */
	/*
	 * the close/open markers are themselves events: no
	 * nested rotation (unbounded recursion)
	 */
	int rotating;
};

int retraced_journal_open(struct retraced_journal *j,
	const char *path);
void retraced_journal_close(struct retraced_journal *j);

/*
 * Rotation + retention (TODO.impl/10). Call BEFORE the first
 * event (open() is fine; the first append resolves the live
 * segment). rotate_bytes/rotate_seconds are triggers (0 =
 * off); budget_bytes caps the PRUNED segments' total (0 =
 * unlimited) -- the live segment is never pruned.
 */
void retraced_journal_set_rotation(struct retraced_journal *j,
	uint64_t rotate_bytes, long rotate_seconds,
	uint64_t budget_bytes);

/*
 * The live segment path for seq `n` of series `base` (writes
 * at most cap bytes). `<base>.<NNNN>`.
 */
void retraced_journal_segment_path(const char *base, int seq,
	char *out, size_t cap);

/*
 * Does segment file `path` exist (and its size, when non-NULL)?
 * Portable probing: no dirent -- segment names are OURS,
 * deterministically numbered.
 */
int retraced_journal_segment_exists(const char *path,
	long long *size_out);

/*
 * The lowest existing segment number for `base` under `hi`
 * (retention prunes from here). Returns -1 when none exist.
 */
int retraced_journal_segment_lo(const char *base, int hi);

/*
 * The surviving segment range [lo, hi] for `base` (-1 when
 * none). Retention removes the OLDEST segments, so the series
 * is contiguous from lo to hi; discovery probes from the top
 * (numbered names are ours, so probing is deterministic).
 */
int retraced_journal_segment_range(const char *base,
	int *lo_out, int *hi_out);

/*
 * Append one event line. `payload` is the EVENT message's JSON
 * object text; the journal wraps it with ts, agent_id, seq and
 * the chain link. Returns 0 ok, -1 io error.
 */
void retraced_journal_flush(struct retraced_journal *j);

int retraced_journal_event(struct retraced_journal *j,
	long ts, const char *agent_id, uint64_t seq,
	const char *payload);

/*
 * Replay the whole journal: re-verify the chain, rebuild
 * registry liveness (HELLO -> live entry, BYE -> gone).
 * Returns 0 (a torn tail is normal), -1 on unreadable file,
 * and sets chain_broken_at when verification fails (the
 * daemon then refuses to start: fail-closed authority).
 */
int retraced_journal_replay(struct retraced_journal *j,
	struct retraced_registry *r);

/*
 * The evidence read arm (the ctl 'events' command rides this):
 * stream the journal's records through a sink, verifying the
 * hash chain exactly as replay does. last_n keeps only the
 * final N records (earlier ones are verified then skipped);
 * chain_out receives 0 when every link held, else the 1-based
 * line number where the chain broke. Returns the number of
 * records emitted through the sink.
 *
 * The integrity verdict travels WITH the evidence: a caller
 * pulling records over a network reports what it was told.
 */
int retraced_journal_tail(struct retraced_journal *j, size_t last_n,
	void (*sink)(const char *line, void *user), void *user,
	long *chain_out);

/*
 * Standalone chain verdict over a journal FILE (TODO.impl/07):
 * recompute every link in order and report the head + line
 * count, with the first broken line number (0 = verified). The
 * signing/verification module and the CLI both ride this --
 * one chain-check arithmetic, three consumers.
 */
int retraced_journal_chain_file(const char *path, size_t stop_at,
	uint64_t *head_out, size_t *lines_out, size_t *broken_at);

/*
 * The chain link of one stored line given its predecessor --
 * the SSOT arithmetic (FNV-1a over the line incl. newline,
 * seeded by prev ^ the mix constant). Replay, chain_file, the
 * signer, and the ctl's query all share this; nothing
 * re-derives it.
 */
uint64_t retraced_journal_line_hash(const char *line,
	uint64_t prev);

/*
 * The segment variant of chain_file (TODO.impl/10): verify one
 * rotated segment starting from its predecessor's final head.
 */
int retraced_journal_chain_file_from(const char *path,
	uint64_t start_prev, size_t stop_at,
	uint64_t *head_out, size_t *lines_out, size_t *broken_at);

#endif /* RETRACE_TOOLS_JOURNAL_H_ */
