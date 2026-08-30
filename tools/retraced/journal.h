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
	char path[512];
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
};

int retraced_journal_open(struct retraced_journal *j,
	const char *path);
void retraced_journal_close(struct retraced_journal *j);

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

#endif /* RETRACE_TOOLS_JOURNAL_H_ */
