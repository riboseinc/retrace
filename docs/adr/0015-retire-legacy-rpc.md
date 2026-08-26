# ADR-0015: Retire the legacy rpc/ subsystem

- Status: Accepted
- Date: 2026-08-26
- Deciders: retrace maintainers

## Context

`rpc/` is a v1-era optional subsystem (gated behind
`RETRACE_ENABLE_RPC`, default `OFF`) that executed intercepted
calls OUT-OF-PROCESS: a frontend library talked to a backend
daemon over a per-call socketpair. It was a debugging aid of the
v1 architecture.

Nothing in the v2 arc builds it: no CI leg turns the gate on,
no test exercises it, no example or tool links it. Through the
entire supervisor arc (TODO.supervisor/01-07) its role has been
superseded by the in-process agent + `retraced` control plane
(ADR series for the supervisor), which supervises rather than
re-hosts execution. The v1 tree itself was already removed at
v2.1.0 (ADR-0011) — `rpc/` survived only because it was
technically compilable standalone.

## Decision

Retire `rpc/` (Option A of TODO.supervisor/10):

1. Delete the `rpc/` subtree and its `Makefile.in`.
2. Remove the `RETRACE_ENABLE_RPC` CMake option, the
   `RTR_RPC` config define, and the `add_subdirectory(rpc)`
   gate.
3. Scrub `RETRACE_ENABLE_RPC` / `rpc/` references from
   CLAUDE.md, README.adoc, and docs/development.md.

The one capability with no v2 equivalent — on-demand target
memory peek (the old GET_MEMORY/GET_STRING pair) — is NOT
carried over: the supervisor's capture_buffer action and the
profile snapshot flow cover the practical cases. If a future
slice wants live peek, it will be designed as a verb in the
supervisor protocol (TODO.supervisor/01), not a revival of the
socketpair stack.

## Consequences

- The tree has ONE remote-control story: the supervisor
  protocol (UDS today, TLS/named pipes per TODO.supervisor/08
  and 12). A security tool carries less unbuilt attack surface.
- Removal is a no-op for every build: the gate defaulted to
  OFF and no leg enabled it (verified by the unchanged CI
  matrix).
- The templates and sources remain recoverable from git
  history at the deletion commit.
- Users who somehow relied on `RETRACE_ENABLE_RPC=ON` builds
  lose an unmaintained debugging aid; the migration path is
  the v2 agent + `retrace-ctl`, or pinning a tag ≤ v2.42.x.

## Alternatives considered

- **Option B (fold memory-peek into the new protocol):**
  deferred; no current consumer wants it. Adding an unused
  verb would be speculative surface — it can be added when a
  slice asks for it.
- **Keep rpc/ dormant:** rejected — carrying dead code in a
  security tool is a liability, and every platform sweep
  (Windows, musl, OHOS) pays a small tax to keep it
  compilable.
