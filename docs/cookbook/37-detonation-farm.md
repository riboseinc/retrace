# 37 — Supervised detonation farm

## Problem

> I need to run hundreds of untrusted samples and know — per
> sample, live — what they touched, with the ability to cut
> network or freeze a cell the moment it misbehaves. Post-mortem
> logs are too late.

`retrace` (≥ 2.39.0) grew a supervisor plane for exactly this:
`retraced` (the daemon), an in-process agent preloaded into
every cell, and `retrace-ctl` as the operator's hand.

## The one-script tour

```
cd examples/supervisor-quickstart
./run-linux.sh ../../build        # macOS: ./run-macos.sh
```

Boot the daemon, detonate under supervision, watch events land,
tighten policy mid-run, freeze, bundle. Every step below is one
line of that script.

## The farm pattern

Each cell is one `exec` under the supervisor env:

```sh
retraced --sock /run/retraced/agent.sock \
         --ctl /run/retraced/ctl.sock \
         --journal /var/log/retrace/journal.jsonl \
         --nonce-file /run/retraced/nonce &

# per sample (the nonce is how the daemon tells your cells
# from a traced process pretending to be one):
RETRACE_SUPERVISOR=1 RETRACE_SUPERVISOR_EAGER=1 \
RETRACE_SUPERVISOR_SOCK=/run/retraced/agent.sock \
RETRACE_SUPERVISOR_NONCE=$(cat /run/retraced/nonce) \
RETRACE_JSON_CONFIG=detonation-default.json \
LD_PRELOAD=libretrace.so ./sample.exe &
```

The operator side is `retrace-ctl`:

| Command | What it does |
|---|---|
| `ps` | Live registry: agent id, session, policy epoch, state |
| `policy-push tight.json` | Tighten EVERY cell mid-run — no restarts |
| `freeze` | Hold all agents: fabricated returns, zero real execution |
| `kill PID` | End one cell |

Process trees are stitched, not guessed: forked and exec'd
children inherit the session token; an exec that strips it is
recorded as a scrub; an untraced hop in the middle leaves a
labeled hole. One detonation = one session id = one trace.

## Mid-run tightening

The moment the first beacon appears:

```sh
cat > cut-net.json <<EOF
{"policy":{"epoch":2},
 "intercept_scripts":[
   {"func_name":"connect",
    "actions":[{"action_name":"addr_deny",
                "action_params":{"mode":"allowlist"}}]},
   {"func_name":"open","actions":[{"action_name":"call_real"}]}]}
EOF
retrace-ctl --sock ctl.sock policy-push cut-net.json
```

Epochs only move forward: a captured POLICY_SET replayed at a
cell is refused.

## Where the events go

Every denial, scrub, and policy ack lands in the daemon's
hash-chained journal — tamper-evident by construction. For a
farm-wide view, set `RETRACE_OTLP_ENDPOINT` and every span
carries the session and agent labels (recipes 23, 36): one
waterfall per detonation tree in your OTel backend.

## Notes

- The transport is local-only today (UDS, peer-credential
  gated); the TLS fleet transport is the next slice — see
  `docs/threat-model-control-plane.md`.
- `freeze` freezes `sleep` too: a frozen cell spins hot. Freeze
  to capture the bundle, then `kill`.
