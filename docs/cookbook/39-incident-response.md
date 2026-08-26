# 39 — Incident response: the specimen hold

## Problem

> A production process is behaving badly RIGHT NOW. I need it
> inert — zero further real syscalls — but alive for memory
> capture, with the last thing it did already on disk. `kill
> -9` destroys both the specimen and the evidence.

Plain retrace cannot offer this: it must be present from
`exec`, and its trace dies with the process. The supervisor
plane (≥ 2.39.0) changes both.

## The hold

```
freeze   every intercepted call returns a fabricated value —
         the process keeps "running" but nothing real happens
ps       registry: which agents live, which session
kill     end the hold after the bundle is captured
```

```sh
retrace-ctl --sock ctl.sock freeze
retrace-ctl --sock ctl.sock ps | head
retrace-ctl --sock ctl.sock kill $PID
```

The freeze is a POLICY, not a signal: it lands as the next
epoch, every agent acks it, and the journal records
`retrace.policy.freeze` with the epoch. From freeze to kill,
the process makes no real intercepted calls — the memory image
you capture between the two is the specimen.

## The bundle

By the time you freeze, the daemon already holds the story:

- the hash-chained journal (denials, session stitching, policy
  epochs, auth decisions),
- the live registry (`ps`) — agent ids, sessions, parentage,
- the target's own captured buffers (recipe 15's
  `capture_buffer` action), if the policy included them.

```sh
retrace-ctl --sock ctl.sock ps > ir-registry.json
cp /var/log/retrace/journal.jsonl ir-evidence.jsonl
# ... memory capture of the frozen process ...
retrace-ctl --sock ctl.sock kill $PID
```

## Freeze semantics — the quiet hold (v2.44.0)

Freeze fabricates returns for every intercepted call EXCEPT the
pure timeouts: `sleep`/`usleep` pass through, so a frozen polling
loop idles instead of spinning (earlier releases spun frozen
targets hot — that amplification is gone). Everything real still
stops: no file, socket, or memory progress. `thaw` exists for
controlled detonation — unfreezing under a tightened policy when
you WANT the next stage to run.

## Future work (honesty section)

- **attach PID** — supervising an already-running process
  (injection/ptrace) is planned, not shipped; today the env
  must be present at exec.
- **ring pull** — a bounded black-box pull of the last N calls
  before the freeze; today the journal + capture buffers cover
  the ground.
- The P1 TLS transport and certificate-scoped controllers are
  what make this a cross-host procedure — see
  `docs/threat-model-control-plane.md`.
