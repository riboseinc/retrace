# supervisor-quickstart (TODO.supervisor/09)

The one-script tour of the retrace control plane: a supervised
detonation from supervisor boot to evidence bundle.

```
./run-linux.sh /path/to/build     # Linux
./run-macos.sh /path/to/build     # macOS
```

Every step is a control-plane verb — no hand-armed env, no
shell-side kills:

1. `retraced` boots with `--policy`: the denial policy rides
   the boot, pushed to every agent as it joins.
2. `retrace-ctl spawn` launches the detonation — supervisor
   env, nonce, and preload in one journaled command (the
   launch arm). No exported environment at all.
3. `ps` shows the join: role `full` (the nonce traveled with
   the fork), policy epoch already applied.
4. `retrace-ctl policy-push` tightens the policy MID-RUN —
   the live target picks it up without restart.
5. `retrace-ctl freeze` holds every agent (the incident-
   response hold: fabricated returns, no real execution).
6. The journal names the story so far: launch, session minted,
   seat taken, denials, policy push, freeze.
7. `retrace-ctl kill` ends the hold — and the departure is
   journaled too (`retrace.ctl.exit`, with how and code).
8. The graceful close flushes the tail: the complete bundle.

Cookbook recipes built on this flow: 37 (detonation farm),
38 (continuous audit), 39 (incident response) under
`docs/cookbook/`.

Note on `freeze`: pure timeouts (`sleep`/`usleep`) pass through
(v2.44.0 quiet hold), so a frozen target sits quiet — end the
hold with `kill` once the bundle is captured.
