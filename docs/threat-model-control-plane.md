# Control-plane threat model (retraced / retrace agent)

Status: P0 (local transport) shipped — TODO.supervisor/08.
P1 (TLS fleet transport) planned. This document answers the
adversary table; every row names the control that answers it.

retrace supervises untrusted code. The control plane (the
`retraced` daemon, the in-process agent, and `retrace-ctl`)
executes policy decisions — deny, freeze, kill — so the plane
itself must be harder to subvert than the workloads it guards.

## Assets

1. **Policy authority** — worst case, an attacker pushes a
   permissive policy: a jail bypass.
2. **Registry + journal integrity** — the audit trail of what
   the supervised fleet did (hash-chained journal).
3. **Host process control** — `spawn`/`kill` reach `SIGTERM`
   on arbitrary supervised pids.

## Adversaries and answers

| Adversary | Capability attempted | Control |
|---|---|---|
| The traced process | Talk to the agent socket directly; forge HELLOs to impersonate an agent | The UDS is mode 0660 (same-uid/group only). A forged HELLO without the channel nonce registers as a **spectator**: events accepted (evidence flows), policy and commands never sent. The nonce is minted by the daemon and handed to spawners out-of-band (`--nonce-file`, mode 0600). HONEST LIMIT (P0): the nonce rides the spawn environment, so the traced process itself can read it; when the deployment must defend against the tracee, the sandbox `deny_env`/`allow_env` jail hides `RETRACE_*` from the traced code at exec. A nonce-armed tracee can additionally forge events into the journal under its own pid's agent id (it cannot alter history: the chain detects rewrites, and policy never flows to it unless it is a legitimate agent). P1 binds the role to the connection (per-connection nonce issued in WELCOME), closing impersonation fully. |
| The traced process | Replay a captured POLICY_SET to confuse epochs | Per-agent policy epochs; agents accept only strictly-greater epochs (TODO.supervisor/05; replay refusal is covered by its tests). |
| A local unprivileged user | Connect to the agent or ctl socket | Accept-time peer credential check (`SO_PEERCRED` / `LOCAL_PEERCRED`): only the daemon's euid or root. Refusals are closed and journaled (`retrace.auth.refused` with peer uid). The ctl socket is 0600 on top. |
| A local unprivileged user (OpenBSD) | Same | No credential query exists on OpenBSD local sockets: the daemon fails OPEN on the liveness plane (agents may connect) — the 0660 mode still gates reachability — and the journal records that the platform gate was unavailable. The ctl plane stays 0600. (Failing closed here would mean refusing all agents on a platform where the mode gate already excludes other users.) |
| A network attacker | Reach the control plane | There is no TCP listener. Local AF_UNIX only (P0). Remote mode (P1) will be TLS 1.3 mutual-auth only; no plaintext remote transport exists or will. |
| A compromised peer controller | Push policy / kill outside its mandate | P0: ctl requires daemon-euid or root (mode 0600 + PEERCRED). P1 adds certificate claim scopes (spawn/policy/kill) enforced per command; every mutating command journals the authenticated peer identity. |
| A compromised daemon | Forge the audit trail | The journal is hash-chained (`prev` links each record); replay detects breaks and the daemon refuses to start on a broken chain (fail-closed). |
| Flooding / misbehaving peer | Exhaust daemon attention | Per-connection frame budgets (1 MiB buffers, `POLICY_MAX_BYTES` inbound policy cap); misbehaving peers lose their connection; spectators are quarantined by role rather than disconnected — evidence keeps flowing (the doctrine: keep the witness alive). |

## The spectator doctrine

An unauthenticated (nonceless) HELLO is evidence, not a threat:
the daemon keeps the connection, accepts events into the
journal, and never sends policy or commands to it. Detonations
under supervision should not be able to silence their own
witnesses — but neither can they steer them.

## What is explicitly NOT covered yet (P1)

- TLS transport, certificate claim scopes, remote controllers.
- Rate/flood token buckets per connection.
- Socket activation + privilege drop.

Until P1 ships, the daemon must not be reachable off-host; the
local deployment (UDS on one machine) is the supported mode.

## Verification

`test/integration/test_supervisor_auth.py` — the negative
suite: socket mode gates, wrong/missing nonce → spectator,
correct nonce → full + policy delivery, cross-uid connection
refused and journaled.
