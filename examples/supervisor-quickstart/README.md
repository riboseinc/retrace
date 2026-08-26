# supervisor-quickstart (TODO.supervisor/09)

The one-script tour of the retrace control plane: a supervised
detonation from supervisor boot to evidence bundle.

```
./run-linux.sh /path/to/build     # Linux
./run-macos.sh /path/to/build     # macOS
```

What you should see, step by step:

1. `retraced` boots and publishes the agent nonce (`--nonce-file`).
2. A small "detonation" runs under `RETRACE_SUPERVISOR=1` +
   preload; every denied path read is a supervised security
   event that lands in the daemon's hash-chained journal.
3. `retrace-ctl ps` shows the registration (role `full`, one
   session).
4. `retrace-ctl policy-push` tightens the policy MID-RUN —
   the live target picks it up without restart.
5. `retrace-ctl freeze` holds every agent (the incident-
   response hold: fabricated returns, no real execution).
6. The journal bundle names the whole story: session minted,
   denial, policy push, freeze.
7. The target is killed after the hold — evidence first.

Cookbook recipes built on this flow: 37 (detonation farm),
38 (continuous audit), 39 (incident response) under
`docs/cookbook/`.

Note on `freeze`: pure timeouts (`sleep`/`usleep`) pass through
(v2.44.0 quiet hold), so a frozen target sits quiet — end the
hold with `kill` once the bundle is captured.
