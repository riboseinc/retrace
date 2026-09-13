# 45 — Farms run thousands: the campaign runner

## Problem

A farm does not run one sample once. It runs N samples under M
policies, repeats for variance, and needs the evidence INDEXED
by run — not one terminal window per detonation.

## Config

`retrace-campaign MANIFEST.json`:

```json
{
  "ctl_sock": "/var/run/retrace/ctl.sock",
  "journal":  "/var/lib/retrace/journal.jsonl",
  "preload":  "/usr/lib/libretrace.so",
  "repeats": 3,
  "concurrency": 8,
  "timeout_sec": 60,
  "samples": [
    { "name": "unpacker", "argv": ["/samples/unpacker", "--all"] },
    { "name": "dropper",  "argv": ["/samples/dropper"] }
  ],
  "policies": [
    { "name": "open-only",    "path": "/policies/open.json" },
    { "name": "deny-shadow",  "path": "/policies/shadow.json" }
  ]
}
```

The manifest is validated WHOLE — a farm does not run half a
plan — and expands to the matrix in a deterministic
sample-major order (for each sample, for each policy, for each
repeat): run indices in the summary and the journal's launch
sequence are reproducible.

## What it does

Each cell: `policy-push` the cell's policy, `spawn` the sample
(both through `retrace-ctl` — the daemon's public surface; the
campaign is a pure client), then wait for the departure by
polling the journal query arm for the run's
`retrace.ctl.exit` record — the pid-keyed verdict
(`exited/0` → CLEAN, `exited/N` → FAIL, `signaled` → CRASH).
The daemon journals every launch and every departure; the
campaign only reads it back.

```
$ retrace-campaign farm.json
campaign: 2 samples x 2 policies x 3 = 12 runs (concurrency 8)
SAMPLE               POLICY            REP      PID  VERDICT      EVIDENCE
unpacker             open-only          0    31227  CLEAN        retrace-ctl events --journal ... --query 'pid == 31227'
unpacker             deny-shadow        0    31230  CRASH        retrace-ctl events --journal ... --query 'pid == 31230'
...
summary: 12 runs: 6 clean, 3 fail, 3 crash, 0 other
```

The EVIDENCE column is the indexed evidence: a ready-to-run
journal query per run. Failures triage in seconds:

```sh
retrace-ctl events --journal /var/lib/retrace/journal.jsonl \
                   --query 'pid == 31230'
```

## Notes

- The campaign exits nonzero if any run TIMED OUT (a farm's
  output must be trustworthy); FAIL/CRASH verdicts are the
  sample's outcome, not the campaign's.
- Everything composes: rotation keeps the journal bounded,
  `verify-journal` proves the series, queries speak the filter
  language (`name ~ "*deny*" and pid == N`).

## Verified

`test/unit/test_campaign_model.c` (validation + the expansion
order — the order IS the spec) and
`test/integration/test_campaign.py` (3×2 against a real
daemon: 6 sessions, verdicts, journal parity, evidence
queries).
