# 25 — Detect performance regressions between two builds

## Problem

You shipped a PR. CI passed. Now users report your binary is 30%
slower. You have yesterday's "good" trace and today's "bad" trace;
reading both side-by-side isn't scaling.

`retrace-diff` reads two retrace traces, groups calls by function,
and prints a structured diff of call counts and total time. With
`--threshold pct=N` you can suppress noise (functions that changed
by less than N%) so the report flags only changes worth looking at.
The exit code doubles as a CI gate: 0 = no diff, 1 = diff above
threshold, 2 = error.

## Config

Use the default wildcard config — you need `call_duration_us` for
every function, which `log_params` emits by default:

`diff-default.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "*",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

## Invocation

### 1. Capture the baseline (good) trace

```sh
$ git checkout v1.4.2  # last known good
$ cmake --build build && retrace run \
    --config cookbook/diff-default.json \
    --log /tmp/before.json \
    -- ./build/your-binary input.bin
```

### 2. Capture the candidate (after) trace

```sh
$ git checkout my-feature-branch
$ cmake --build build && retrace run \
    --config cookbook/diff-default.json \
    --log /tmp/after.json \
    -- ./build/your-binary input.bin
```

### 3. Run the diff

```sh
$ retrace-diff /tmp/before.json /tmp/after.json

function: malloc
  count:      before=1247, after=1532  (+285, +22.8%)
  total time: before=8.3ms, after=11.9ms (+3.6ms, +43.4%)

function: open
  absent in before; 3 calls in after

function: pthread_mutex_lock
  count:      before=42000, after=42103  (+103, +0.2%)
  total time: before=15.1ms, after=15.2ms (+0.1ms, +0.7%)
```

### 4. CI gating with a threshold

Suppress everything below 10% so noise (mutex locks, time()) falls
out of the report:

```sh
$ retrace-diff --threshold pct=10 /tmp/before.json /tmp/after.json
function: malloc
  count:      before=1247, after=1532  (+22.8%)
  total time: before=8.3ms, after=11.9ms (+43.4%)
$ echo $?
1
```

Exit code 1 means "diff found above threshold" — wire it into CI:

```sh
#!/bin/bash
# scripts/check-perf-regression.sh
retrace-diff --threshold pct=10 /tmp/before.json /tmp/after.json
rc=$?
if [ $rc -eq 1 ]; then
  echo "::error::Performance regression detected (see diff above)"
  exit 1
fi
exit 0
```

## Expected output

A plain-text report, one block per function whose call count or
total time changed (or appeared/disappeared). With `--threshold`,
only functions whose percentage change exceeds N% appear.

Exit codes:

| Code | Meaning |
|------|---------|
| 0 | No diff above threshold (CI: pass) |
| 1 | Diff found above threshold (CI: fail) |
| 2 | Argument or IO error (CI: fail with bug in the diff setup) |

## Variations

### Statistical significance (--stats)

A single before/after run is noisy. Run the baseline N times to
establish a distribution, then flag functions whose test run
deviates by more than N standard deviations:

```sh
$ for i in 1 2 3 4 5; do
    retrace run --config cookbook/diff-default.json \
        --log /tmp/baseline-$i.json -- ./build/your-binary input.bin
done
$ retrace run --config cookbook/diff-default.json \
    --log /tmp/test.json -- ./build/your-binary input.bin

$ retrace-diff --stats /tmp/baseline-1.json,/tmp/baseline-2.json,/tmp/baseline-3.json,/tmp/baseline-4.json,/tmp/baseline-5.json /tmp/test.json
function: malloc
  baseline: mean=1245.2, stddev=12.4 (n=5)
  test:     1532
  z-score:  +23.1  (significantly higher; ~99.99% confidence)

function: open
  baseline: mean=11.0, stddev=0.0 (n=5)
  test:     11
  z-score:  0.0  (no change)
```

Tune the z-score threshold with `--zscore N` (default 2.0, ~95%
confidence).

### One-shot diff after a trace pair

Skip the file intermediary when both runs go through the same shell:

```sh
$ diff <(retrace run --config cookbook/diff-default.json -- ./old-binary \
          2>&1 >/dev/null) \
      <(retrace run --config cookbook/diff-default.json -- ./new-binary \
          2>&1 >/dev/null)
```

(Use the file-based approach for CI — it's easier to debug.)

## Caveats

- Total-time comparisons assume the same machine and similar load.
  For CI, pin to dedicated runners and warm up the page cache
  before each run.
- The diff is by function name, not by call site. Two call sites of
  the same function aggregate together; use recipe 17 (per-return-
  address routing) if you need finer granularity.
- `--stats` needs at least 2 baseline files to compute stddev;
  fewer and it falls back to "any change is significant".

## See also

- Recipe 04 — Time each call (for the underlying timing data).
- Recipe 26 — Call-order diff (`--order`, the LCS-based variant).
- Recipe 19 — CI fuzzing (a different CI pattern: bug-finding, not regression).
