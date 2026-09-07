# Supervisor policy templates

Ready-to-push policies for `retraced` (TODO.supervisor/09 P1).
Each file is a complete `retrace-ctl policy-push` artifact: the
daemon's loader validates it as-is. (These are supervisor
policies — `intercept_scripts` — not the profiler's risk-scoring
rulesets in `../policies/`.)

| Template | What it holds |
|----------|---------------|
| `read-only-workload.json` | The detonation default: write-class calls denied outright (mode/flags derived), credential paths fenced (`/etc/shadow`, ssh keys) |
| `no-network.json` | `socket`/`connect` synthesized −1 + the secret env names fenced |
| `env-fence.json` | getenv on common token/key names returns NULL — credential exposure without touching the filesystem |
| `decoy-farm.json` | Deny-by-default jail (`allow_paths`) with `decoy_dir`: out-of-fence READS get plausible fakes instead of denials, keeping the sample on its happy path (the deception is logged). Edit the paths to the detonation host first |

## Pushing

```sh
retrace-ctl --sock /tmp/retraced.ctl.sock policy-push read-only-workload.json
```

**The epoch ladder:** agents only ever accept a strictly greater
epoch, and every template ships at `epoch: 1` for a cold boot
(`retraced --policy read-only-workload.json`). On a warmed
daemon, bump the epoch in your copy before pushing — or sign
your edited ladder:

```sh
retrace-ctl sign-policy my-edit.json ed25519.key > signed.json
retrace-ctl --sock ... policy-push signed.json
```

## Kernel truth

A `sandbox` denial is observable without reading retrace's
output: the blocked call returns EACCES (or the synthesized
value), and every denial lands in the daemon's hash-chained
journal (`retrace.jail.denied`) and, when OTLP is configured,
live in the collector.
