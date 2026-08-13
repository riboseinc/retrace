# 26 — Diff the call-order between two runs

## Problem

Same function call counts. Same total time. But the program behaves
differently. The issue is order — one run does `open → read → close`
and the other does `open → close → read`. Per-function aggregation
hides this.

`retrace-diff --order` runs a longest-common-subsequence (LCS) diff
across the call sequence (not the per-function stats). Output looks
like `git diff` for traces: lines marked ` ` (both), `<` (only
before), `>` (only after). Same exit-code contract as the basic
diff (0/1/2).

## Config

The default wildcard config works. You don't need `call_duration_us`
for the order diff (only `message.func`), but mixing both gives a
single config that works for both diff modes:

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

### 1. Capture both traces

```sh
$ retrace run --config cookbook/diff-default.json \
    --log /tmp/before.json -- ./your-binary input-a.bin
$ retrace run --config cookbook/diff-default.json \
    --log /tmp/after.json  -- ./your-binary input-b.bin
```

### 2. Run the order diff

```sh
$ retrace-diff --order /tmp/before.json /tmp/after.json
  open    path=/etc/foo.conf
  read    fd=3
< write   fd=3  buf="deprecated branch"
  read    fd=3
> fstat   fd=3
  close   fd=3
```

Interpretation:

- ` ` (space) — call appears in both, in the same relative order.
- `<` — appears only in the before trace (input-a triggered a code
  path that the after trace did not).
- `>` — appears only in the after trace.

### 3. Combine order + threshold for CI gating

```sh
$ retrace-diff --order --threshold pct=5 \
    /tmp/before.json /tmp/after.json
$ echo $?
1   # CI: fail when the order diverges meaningfully
```

## Expected output

A unified-diff-style block showing the LCS alignment of the two
call sequences. Each line is one call, prefixed with the alignment
marker.

## Variations

### Filter to a subset of functions

Pre-filter the trace with `jq` to restrict the LCS to just the
calls you care about (e.g. only `open`/`read`/`write`/`close`):

```sh
$ jq -c 'select(.message.func == "open" or .message.func == "read" or
                .message.func == "write" or .message.func == "close")' \
    /tmp/before.json > /tmp/before-iopc.json
# (repeat for /tmp/after.json)
$ retrace-diff --order /tmp/before-iopc.json /tmp/after-iopc.json
```

This is sharper than running the full LCS on a noisy trace —
parsing/decoding calls that always happen in deterministic order
drown out the IO pattern.

### Compare against a golden trace

Check in a "golden" trace and run the order diff on every PR:

```sh
# .github/workflows/trace-regression.yml
- name: Compare trace against golden
  run: |
    retrace run --config cookbook/diff-default.json \
        --log /tmp/candidate.json -- ./build/your-binary test-input
    retrace-diff --order test/golden.json /tmp/candidate.json
```

Fail the job on any divergence — order changes are often the
signature of a behavior change that stats-only diff misses.

## Caveats

- LCS is `O(n*m)` in trace length. For traces over ~50K calls
  each, restrict to a function subset first (see Variations).
- The diff is on call identity (function name + same call index
  in the matching run), not on arguments. Two `open("/etc/a")`
  calls and two `open("/etc/b")` calls align by name only.
- Engine-noise entries (those with `message.text` instead of
  `message.func`) are filtered out before the LCS — they don't
  represent real intercepted calls.

## See also

- Recipe 25 — Performance regression diff (the per-function stats
  variant; use both together).
- Recipe 17 — Per-return-address routing (when the same function
  needs different behavior at different call sites).
- Recipe 24 — Audit a trace (run a policy over either trace to
  explain why the order differs).
