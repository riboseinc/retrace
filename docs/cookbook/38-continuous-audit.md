# 38 — Continuous audit: attestable "what was in force when"

## Problem

> Compliance asks: prove what a production service was ALLOWED
> to do last quarter, prove what it actually did, and prove the
> evidence wasn't edited. An editable local log answers none of
> that.

The supervisor plane (≥ 2.39.0) makes retrace output
audit-grade:

- **Policy as a versioned artifact.** The policy a service runs
  under is a file with an epoch; the daemon only ever moves
  epochs forward and refuses replays.
- **Every violation is an event.** A supervised process that
  touches a denied path emits `retrace.jail.denied` — live,
  not post-mortem.
- **The journal chains itself.** `retraced --journal` writes a
  hash-chained JSONL: each record cites the previous record's
  digest. Edit line 400 and every later `prev` breaks; the
  daemon refuses to start on a broken chain (fail-closed).

## The supervised-service setup

```sh
retraced --sock /run/retraced/agent.sock \
         --ctl /run/retraced/ctl.sock \
         --policy baseline-policy.json \
         --journal /var/log/retrace/journal.jsonl \
         --nonce-file /run/retraced/nonce &

RETRACE_SUPERVISOR=1 RETRACE_SUPERVISOR_EAGER=1 \
RETRACE_SUPERVISOR_SOCK=/run/retraced/agent.sock \
RETRACE_SUPERVISOR_NONCE=$(cat /run/retraced/nonce) \
RETRACE_JSON_CONFIG=baseline-policy.json \
LD_PRELOAD=libretrace.so /usr/sbin/the-service
```

`--policy` pushes the policy to every agent at registration and
on each re-HELLO; agents ack with the epoch they hold.

## The quarterly export

```sh
retrace-ctl --sock ctl.sock ps > quarter-registry.json
cp /var/log/retrace/journal.jsonl quarter-evidence.jsonl
sha256sum quarter-evidence.jsonl >> chain-of-custody.txt
```

- `quarter-registry.json` — the supervised population and each
  agent's last policy epoch.
- `quarter-evidence.jsonl` — the chained event stream: denials,
  policy pushes, freezes, auth decisions (`retrace.auth.*`).
- Verification is mechanical: recompute each record's digest and
  compare to the next record's `prev`. Any mismatch localizes
  the edit to one record.

## "What was in force when"

Every `retrace.policy.pushed` and `retrace.policy.freeze` event
carries the epoch; every `retrace.jail.denied` carries the
agent and its session. Policy file + journal together answer the
auditor's question exactly: at timestamp T, the service ran
epoch E, and here is everything E denied.

## Identity of the plane itself

The journal also records who talked to the daemon: peer-uid
refusals (`retrace.auth.refused`), the role of every agent
(`retrace.auth.agent` — `full` or `spectator`). See
`docs/threat-model-control-plane.md` for the full adversary
table and the honest limits of the current (local) transport.

## Notes

- Signed policies (the compliance-grade variant) are planned
  work — today the artifact is a file, and the journal proves
  when each epoch landed, not who signed it.
- Long-running services should supervise from exec (the env
  must be present at process start); attaching to a running
  PID is the incident-response flow (recipe 39).
