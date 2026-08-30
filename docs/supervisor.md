# The supervisor: retraced, retrace-ctl, and the enforcement planes

retrace grew past one process: a per-host supervisor (`retraced`) owns
every traced process — registry, versioned policy, a hash-chained
journal, and live command channel — while telemetry keeps flowing
through the existing OTLP plane. This page is the reference for the
supervisor, its CLI (`retrace-ctl`), the kernel-enforcement compiler
(`retrace-profile enforce` + `retrace-enforce`), and the observation
lanes that report what libc cannot see.

The doctrine, in one line each:

- **Two planes**: control (low-rate, request/response + events) and
  telemetry (OTLP, unchanged) never conflate.
- **Fail-open liveness, fail-closed policy**: a dead supervisor never
  kills a traced process, but the last-applied jail stays in force
  forever.
- **Find with kernel truth, attribute with the supervisor, act with
  the preload**: only the preload plane rewrites arguments; kernel
  observation is spectator-only; kernel enforcement is coarse and
  fail-closed.

## retraced — the daemon

```sh
retraced [--sock PATH] [--journal PATH] [--policy FILE] [--ctl PATH]
         [--nonce HEX32 | --nonce-file PATH]
         [--tls-listen HOST:PORT --tls-cert PEM --tls-key PEM --tls-ca PEM]
         [--user NAME] [--group NAME] [--fd N]
```

| Flag | Meaning |
|------|---------|
| `--sock` | agent socket (UDS; default `/tmp/retraced.agent.sock`) |
| `--journal` | the hash-chained JSONL journal |
| `--policy` | a policy file pushed to every registering agent |
| `--ctl` | local controller socket (newline-JSON; PEERCRED-gated) |
| `--nonce` / `--nonce-file` | the agent channel nonce; spawners inject it into the target env. No nonce in HELLO ⇒ **spectator** (evidence, never policy) |
| `--tls-*` | the fleet control plane: TLS 1.3 mutual auth only, all four flags together or none. Controller certs carry claim scopes in a URI SAN (`retrace:scope:status+ps+policy+kill`). No plaintext remote mode exists |
| `--user` / `--group` | privilege drop after every socket is bound; a failed drop exits (never continues elevated) |
| `--exit-after N` | self-terminate after N seconds — the harness guard: a daemon orphaned by its test/CI run can never outlive its purpose |
| `--fd N` | socket activation: serve an already-bound listener (systemd's `LISTEN_FDS` convention maps here). Inherited sockets are never unlinked |

The journal is append-only and hash-chained; control-plane records
(auth decisions, policy pushes, sessions, drift) are flushed the moment
they are written, routine telemetry is buffered, and an unclean shutdown
leaves a recorded gap (`retrace.journal.unclean`) — never a silent one.
Reboot replays and rebuilds; a broken chain refuses to start.

## retrace-ctl — the fleet CLI

```sh
retrace-ctl [--sock PATH] COMMAND
retrace-ctl --tls-host H:P --tls-cert PEM --tls-key PEM --tls-ca CA COMMAND
```

Commands: `status`, `ps`, `policy-push FILE`, `freeze`, `thaw`, `kill
PID`. Every command maps to a claim scope; a peer whose cert lacks the
bit is refused with `scope denied` and the attempt is journaled as
`retrace.auth.overscope`. Local UDS peers hold all scopes (PEERCRED
already gated the accept).

## The observation lanes

One session, one journal, three lanes:

- **libc** — the preload agent: what the target *claimed* it did.
- **kernel** — `retrace-ebpf-agent` / `retrace-etw-agent`: what the
  kernel *saw*. Both join as spectators (nonceless HELLO on purpose)
  and emit `source: kernel` events. `--synthetic` proves the protocol
  on CI; `--loader` wraps the real bridge (`retrace-ebpf-loader`,
  `etw2retrace`). On Windows both speak
  the named pipe natively (pass `--sock \\.\pipe\...`).
- **runtime** — `pyretrace` / `jretrace`: which module, which line.
  See [runtime agents](runtime-agents.md).

The daemon grades drift live: kernel observations with no matching
libc claim journal as `retrace.drift.summary` per agent per sweep —
the correlate oracle's signal, streaming.

## Kernel enforcement: enforce

```sh
retrace-profile enforce <profile.json> [--inside declared.json]
        [--backend landlock|seccomp|sandbox-exec|appcontainer|all]
        [--exec PATH] [-o spec.json]

retrace-enforce [--allow-missing] [--audit TRAIL] [--audit-key PEM]
        spec.json -- cmd [args...]
retrace-enforce --verify-audit TRAIL [--audit-pubkey PEM]
```

One declared-set, four enforcement planes, graded against each other:

| Plane | Host | Shape |
|-------|------|-------|
| Landlock | Linux 5.13+ | path-precise read/write/execute rules |
| seccomp | Linux | coarse syscall floor (unsafe classes denied unless observed) |
| sandbox-exec | macOS | Seatbelt S-expressions; both symlink spellings of `/tmp`, `/var` |
| AppContainer | Windows | deny-by-default container; declared paths become DACL grants; no capabilities = no network |

Fail-closed everywhere: a missing plane aborts the exec unless
`--allow-missing` (dev only).

### Windows service

On Windows, `retraced` is one binary with two launches: started by
the SCM it dispatches as a service; started from a console it runs
the accept loop directly. Install it the SCM way:

```powershell
sc create retraced binPath= "C:\path\retraced.exe --sock \.\pipe\retraced-agent --ctl \.\pipe\retraced-ctl --journal C:\ProgramData\retrace\journal.jsonl" start= auto
sc start retraced
```

`SERVICE_CONTROL_STOP` flips the same stop flag the console
Ctrl handler uses, so `sc stop` rides the graceful journal-flush
exit; `--exit-after N` still applies as the watchdog belt under
both launches.

### The audit trail

`--audit TRAIL` binds every exec to `(timestamp, pid, spec digest,
backends, argv)` in a hash-chained JSONL record — the auditor's answer
to *which filter was in force when*. With `--audit-key` (Ed25519) each
record is signed over the exact bytes the chain hash covers;
`--verify-audit --audit-pubkey` requires a valid signature on every
record. A tampered, torn, or unwritable trail refuses the exec.

## Policy signing & rotation

`retrace-ctl sign-policy FILE KEY` wraps a policy in an Ed25519
signature over its exact bytes; the agent verifies against the key
pinned in `RETRACE_SUPERVISOR_PUBKEY` and refuses invalid or partial
wrappers fail-closed. For rotation, pin a LIST of PEMs (path
separator `:` on POSIX, `;` on Windows) — old and new keys overlap,
so agents verify throughout the transition without redeploying.

## Where to read next

- [Runtime agents](runtime-agents.md) — write your own lane
- [Control-plane threat model](threat-model-control-plane.md) — the
  adversary table behind PEERCRED, nonces, scopes, and TLS
- [Reports reference](reports.md) — every output shape
- [Architecture](architecture.md) — engine, backends, actions
