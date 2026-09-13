# 44 — Months of evidence: journal rotation, retention, query

## Problem

The compliance persona's journal grows for months. Appending
forever means one unmanageable file; deleting old evidence
means silent gaps; finding anything means grep by hand.

## Config

The daemon gains three flags:

```sh
retraced --journal /evidence/journal.jsonl \
         --journal-rotate-bytes 16777216 \   # 16 MiB segments
         --journal-budget-bytes 1073741824 \ # keep ~1 GiB
         --journal-rotate-seconds 86400 ...  # also rotate daily
```

Rotation produces a numbered series — `journal.jsonl.0000`,
`journal.jsonl.0001`, ... — and the chain CONTINUES across
segments: each new segment opens with a chained
`retrace.journal.segment_opened` record carrying its
predecessor's final head, and each closed segment ends with
`retrace.journal.segment_closed` (head + line count). One
chain, split across files.

Retention caps the SUM of closed segments
(`--journal-budget-bytes`); the OLDEST segments are pruned, the
live segment never is, and every prune is itself a chained
`retrace.journal.segment_pruned` record — gaps are auditable,
never silent.

## Query

The same expression language the config's filters speak, over
the journal — fields resolve against each record (the event's
own fields, then the envelope's `ts`/`agent`/`seq`):

```sh
retrace-ctl events --journal /evidence/journal.jsonl \
                   --query 'name ~ "*deny*" and agent == "agent-7"'
```

Records stream chain-verified: a broken chain refuses the
query rather than returning partial evidence. Verify the whole
series:

```sh
retrace-ctl verify-journal /evidence/journal.jsonl
# journal.jsonl.0002: chain ok, 6 lines, head e8cd6d...
# ...
# 5 segments verified
```

The oldest surviving segment may report a pruned predecessor —
that is the retention record talking, not corruption.

## Semantics worth knowing

- Rotation off (no flags) is exactly the single-file journal of
  old — nothing changes for existing deployments.
- Boot replays the whole series in order: one chain, every
  cross-segment link verified, a torn tail tolerated only on
  the live segment. A missing or tampered MIDDLE segment
  refuses the boot (fail-closed).
- The seal (`--signing-key`) composes with rotation at the
  daemon's graceful close; sealing each rotated segment at
  rotation time is the follow-up.

## Verified

`test/unit/test_journal_rotation.c` (rotation, cross-segment
links, retention records, tamper-fail-closed) and
`test/integration/test_journal_lifecycle.py` (the real daemon:
segments, verify-across, query == grep parity, pruned oldest +
records). ADR-0017 records the chain-continuity decision.
