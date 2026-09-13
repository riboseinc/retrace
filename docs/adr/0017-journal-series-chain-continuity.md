# ADR-0017: The journal series — one chain, split across segments

- Status: Accepted
- Date: 2026-09-13
- Deciders: retrace maintainers

## Context

The supervisor journal (TODO.impl/10) is append-only JSONL with
a per-line FNV-1a hash chain; the daemon replays it at boot and
refuses to start on a broken chain (fail-closed authority).
Months of evidence in one file is unmanageable, but rotation
and retention both threaten the chain's central property:
verification must not silently degrade into "each file checks
out, who knows about the gaps".

Two structural options existed:

1. **One chain, literally one file forever** — no rotation;
   the compliance story dies under its own weight.
2. **Independent per-segment chains** — each file verifies from
   zero; links between files recorded as data. Verification of
   the SERIES reduces to trusting the link records without
   cryptographic continuity.
3. **One chain, split across files** — each new segment's
   genesis line carries the predecessor's final head as its
   `prev` link; the chain arithmetic is unchanged, only the
   storage is sharded.

## Decision

**One chain, split across segment files.**

- Segments are `<base>.<NNNN>`, numbered by the writer;
  probing is deterministic (no directory listing — the names
  are ours, and Windows dirent is not a dependency we want).
- Rotation (size or time trigger, checked per append) closes
  the live segment with a chained
  `retrace.journal.segment_closed` record (head + lines) and
  opens the next with `retrace.journal.segment_opened`
  (segment, prev_segment, prev_head). The genesis `prev` IS
  the cross-segment link: a verifier recomputes it like any
  other line.
- Boot replays the series in order, carrying the head across
  files. A torn tail is tolerated only on the LIVE segment; a
  missing or tampered closed segment fails the boot. Rotation
  is reentrancy-guarded: the close/open markers are themselves
  events and must not re-trigger rotation.
- **Retention prunes oldest segments only**, under a byte
  budget on the SUM of closed segments; the live segment is
  never pruned. Each prune appends a chained
  `retrace.journal.segment_pruned` record (segment, bytes) to
  the live chain. A pruned gap is therefore announced by the
  chain itself: the oldest surviving segment's genesis
  references a predecessor the reader no longer has, and the
  prune record names it.
- Verification of a surviving series: each segment verifies
  INTERNALLY from its declared genesis link, then the declared
  link is compared with the predecessor's final head — match
  is the live chain; a mismatch on the OLDEST survivor is the
  retention record; a mismatch mid-series is corruption.

## Query

`retrace-ctl events --journal BASE --query EXPR` streams
chain-verified records matching a filter expression — the SAME
parser, tree, and glob as the config's `filter` action
(TODO.impl/08), re-hosted on a JSON-field resolver (the
resolver seam in `filter_dsl.h`: one language, many value
domains). A broken chain refuses the query rather than
returning partial evidence.

## Consequences

- Verifiability is preserved across rotation and retention by
  construction: every structural event (open, close, prune) is
  an ordinary chained record.
- `verify-journal` and the query walk probe the surviving
  range [lo, hi] rather than assuming the series starts at 0000.
- The ed25519 seal composes at the daemon's graceful close;
  seal-at-rotation is the follow-up (the close record already
  carries the digest base).
- Rotation off is byte-for-byte the single-file journal of old.

## Alternatives considered

- **Per-segment independent chains** (option 2) — simpler
  retention, but cross-segment ordering rests on data records
  rather than chain arithmetic; the "one history" property the
  compliance persona depends on would be an assertion, not a
  verification.
- **A real index (offsets, sqlite) for queries** — deferred:
  the compiled-predicate scan evaluates months of JSONL in
  seconds and keeps the format the only source of truth. When
  a real index earns its complexity, it can be derived from
  the series (the journal stays the SSOT).
