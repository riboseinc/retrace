# Changelog

All notable changes to retrace are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
(see `docs/adr/0006-semantic-versioning.md`).

## [2.108.0] — 2026-09-22

**cards 24-26: android rebind infra, ios lane doc, ppc64le packages**

android_rebind.{c,h}: the target-call PLT/GOT rebind, opt-in
via RETRACE_ANDROID_REBIND=1 — the maps-walk module finder,
the APA1 packed-reloc unpacker, and the __retrace_wrap_<func>
alias scheme with the export policy inverted
(global: __retrace_wrap_*; local: *;). docs/ios.md: the
observer-only iOS lane (simulator / frida-bridge / network
observer). Packaging: ppc64le deb/rpm stamped from the target
arch with a cross-build release leg (44 assets, 6 packages).

## [2.107.0] — 2026-09-22

**the docs and site catch up to v2.106.0**

PPC64LE in the platform matrix and the cross-arch docs; the
site feed, badge, and about page current through v2.106.0.

## [2.106.0] — 2026-09-21

**ppc64le -- the fifth ELF backend runs end to end**

Two ELFv2 ABI laws fixed (the caller-LR slot at SP+16 is the
callee's; the local-entry TOC restore), 16-byte trampoline
alignment with a CI gate, the sysv-hash + version-script link
keepers, the inventory-conformance entry, and the ppc64le CI
lane. Validated natively on a real ppc64le kernel.

## [2.105.0] — 2026-09-20

**the docs and site catch up to v2.104.0**

CHANGELOG entries 2.41.0 through 2.104.0; platform docs for
mips64, rv64, and Android/bionic; the website feed, badge,
about paragraph, and docs page current.

## [2.104.0] — 2026-09-20

**Android E2E -- the library runs on bionic**

Weak+hidden trampolines (android.ver global: *;) plus the
linker-free real-impl resolver (real_linkmap.c __ANDROID__
branch: /proc/self/maps + direct DT_GNU_HASH parse, no dlsym)
let binaries run to completion under LD_PRELOAD on the bionic
runtime: config parses, the JSON trace flows end to end, rc=0.
Android v1 tracing semantics are self-interposition plus the
explicit registry API.

- android: the library runs on bionic -- hidden trampolines, linker-free resolution

## [2.103.0] — 2026-09-20

**The Android/bionic cross-build lands (#848): four root fixes**

in shared code -- the dl_iterate_phdr real-impl fallback for
bionic (plus the mmap-floor dlsym guard and vdso skip now
shared by both implementations), weak trampoline binding for
crtbegin's atexit/pthread_atfork, plain section names for
lld's __start_/__stop_ synthesis, and --no-gc-sections for
the registry sections. Runtime bring-up under qemu follows.

- android: the bionic preload library cross-builds

## [2.102.0] — 2026-09-18

**The RISC-V rv64 preload backend ships: full interposition under**

qemu-user verified live, the conformance gate now enforces the
whole .def on all four ELF backends (the mips64 entry had been
silently missing), and the rv64 qemu lane runs permanently on
CI.

- riscv64: the rv64 preload backend

## [2.101.0] — 2026-09-18

**The MIPS64 (big-endian n64) backend comes alive: the n64**

trampoline runs under qemu-user with full log_params +
call_real interposition, policy denial enforced, the
trampoline-alignment gate green, and the qemu E2E lane
permanently on CI (.github/workflows/mips64.yml, ARM runners).
Carries the DataType.value_size big-endian param fix and the
hidden-visibility engine addressing the trampoline needed.

- mips64: one backslash, not two, in the escaped-path grep
- mips64: assert the wire form of the path (parson \/-escapes)
- mips64: the qemu lane runs on ARM runners
- mips64: capture the probe's rc and stderr in the E2E
- mips64: the E2E asserts on the JSON trace only
- mips64: the E2E asserts on the stdout rendering
- ... and 4 more (see git history)

## [2.100.0] — 2026-09-18

**Card 17 (TODO.impl/17): coverage joins the fuzz-report**

signature. The engine's per-thread call-hash (RETRACE_CALL_HASH)
folds into the cluster id -- same-signature deaths with
different call histories separate; a lane carrying no hash
never separates. Two tool gaps fixed on the way: --config
never reached the child, and assertion clusters exited 0 (a
CI gate must fail on any finding). The E2E separates two
same-signature deaths by history and reports the minimized
corpus; the libFuzzer custom-mutator template pulls tokens
from the fuzz_str dictionary format -- one vocabulary for the
workbench and libFuzzer.

- fuzz-report: coverage joins the signature -- the loop closes

## [2.99.0] — 2026-09-14

**Card 18 (TODO.impl/18): live hit-level drift grading. The**

daemon names WHAT escaped beside the counts -- retrace.drift.hit
carries the op and path of every kernel observation no libc
claim covers, deduped so a hot escaping loop names itself once.
The matcher is pure (standalone unit); the seam is daemon_frame's
EVENT arm with suffix-rule key extraction, one site for both
transports. The E2E runs both grading planes on one corpus:
the live daemon names the escape; retrace-correlate, offline,
agrees.

- retraced: live hit-level drift grading -- the daemon names WHAT escaped

## [2.98.0] — 2026-09-14

**Card 13 (TODO.impl/13): the TLS content lane -- what was**

exfiltrated, not just where. The OpenSSL surface joins the
inventory (SSL_CTX_new, SSL_read/write and the _ex variants
that carry OpenSSL 3's data path); tls_content summarizes the
plaintext through the redaction-aware agent fan-out; tls_keylog
injects SSL_CTX_set_keylog_callback so every secret line lands
in RETRACE_TLS_KEYLOG.

Two real main bugs fixed on the way: the dispatch-path lookups
logged (func_get, datatype_get) and re-entered vfprintf through
the interposed localeconv -- the locale lock recursed and any
python trapped at boot; and the real-impl seam could not see
host-loaded providers (RTLD_NEXT searches only the caller's
dependency chain) -- the link-map walk resolves them, skipping
the PIE main whose empty name slips the l_addr guard.

Open bug recorded on the card: the TLS E2E client aborts at
python's create_gil on ubuntu-22.04 x86_64 only (loud scoped
skip; every other flavor passes).

- tls E2E: a loud, scoped skip for the known 22.04-x64 cell
- linkmap walk: the PIE main executable has a name (none) and a bias
- tls E2E: the client joins lazily
- the real-impl seam learns host-loaded providers (the link-map walk)
- tls E2E: name the server's protocol floor
- tls E2E: the server context takes the code-scanning-safe form
- ... and 1 more (see git history)

## [2.97.0] — 2026-09-14

**Card 12 (TODO.impl/12): the Go runtime lane. goretrace speaks**

the same RTRD protocol as the Python/Node/JVM agents on a
cgo-free hook surface -- Supervise/HookHTTP/Emit, both
transports (UDS + the named pipe as a file). The E2E runs one
netcgo process through both evidence lanes: runtime HTTP
events journaled as a full peer, preload-scoped libc calls
beside them (the cross-platform lane is cgo marshaling --
malloc/free; darwin also routes writes through libSystem).

Known gap recorded on the card: full-inventory interposition
under a cgo Go binary trips value-result hazards (getsockopt
EFAULT, interposed getaddrinfo breaks resolution) -- the
out-param prototypes need work in retrace core before
interposition-heavy Go tracing.

- test: the Go libc lane rides cgo traffic, not platform quirk
- goretrace: the Go runtime lane

## [2.96.0] — 2026-09-13

**Card 11 (TODO.impl/11): the audited launch plane on Windows --**

lifecycle parity with the POSIX seam. The daemon's spawn verb
drives the win-run injection machinery (the inject split: a
non-blocking spawn with an env block), the workload joins
through the pipe with EAGER (now a boot-time join on Windows),
and every departure lands in the journal through the handle
reap (SIGCHLD's analogue).

The card also surfaced and fixed three older defects: the
journal's argv0 was not JSON-escaped (a Windows path made the
record unparseable), EAGER existed only in the POSIX agent,
and -- the big one -- the pipe daemon's accept loop blocked in
ConnectNamedPipe on its synchronous handle after the first
client, killing every cadence riding that loop (drift,
registry sweep, reap). The accept now owns a dedicated thread;
the main loop is the pure sweep cadence.

- retraced: the agent accept owns its thread; the cadence owns the loop
- retraced: the connect branch bisects itself under trace
- retraced: the accept loop traces every turn (RETRACED_TRACE)
- test: the spawn E2E reads the workload's last words, and the liveness probe proves its own honesty
- retraced: tracing rides the journal, not stdout
- retraced: the sweep narrates when asked (RETRACED_TRACE)
- ... and 6 more (see git history)

## [2.95.0] — 2026-09-13

**Campaign orchestration (PR #831): the farm arm.**

retrace-campaign turns a manifest into a matrix of runs --
samples x policies x repeats, validated whole, expanded in a
deterministic order -- driven through the daemon's public
surface with pid-keyed verdicts (CLEAN/FAIL/CRASH from the
journal's reap records) and indexed per-run evidence queries.
Building it also delivered the filter DSL's string equality
(dynamic like-typed comparison, no coercion) and shell-safe
command construction. Recipe 45.

- the farm arm: campaign orchestration

## [2.94.0] — 2026-09-13

**The journal lifecycle (PR #829, ADR-0017): one**

chain, split across segment files. Rotation (size/time
triggers) closes and opens segments as chained records;
retention prunes the oldest under a byte budget with every
prune a chained record -- gaps auditable, never silent; the
query arm speaks the filter expression language over the
series, chain-verified on the way through. Recipe 44.

- ctl: bind the tool's real_impls lazily -- MSVC's constant rule
- one chain, split across files: the journal lifecycle

## [2.93.0] — 2026-09-13

**The filter expression language (PR #827): queries as**

config. The filter action's expr param speaks a small predicate
language -- params, globs, comparisons, and/or/not -- compiled
once and validated at config load, with a bad expression
refusing the whole file. Recipe 43, the action reference, a
seeded property suite, and a live E2E on every preload lane.

- queries as config: the filter expression language

## [2.92.0] — 2026-09-13

**The syscall-lane actions (PR #825, ADR-0016): the**

arch-spec seam made polymorphic, and with it the ptrace lane
gains its actions -- a static detonation is now contained, not
merely watched. sandbox denies open("/etc/shadow") with
-EACCES at the syscall stop from the same JSON scripts the
preload lanes run; call_real means allow (the kernel IS the
real implementation); modify-after-allow lands at the exit
stop; log_params reads paths out of the tracee. The skip rides
a benign getpid rewrite delivered at syscall-exit -- immune to
the container seccomp filters that turn invalid syscall
numbers into ENOSYS. Recipe 42, platforms.md, and the deepest
spec set of the arc: the standalone seam unit plus the live
deny E2E on every Linux leg.

- syscall-lane skip: the benign syscall, on every kernel
- ptrace lane: the skip value logs at INFO
- ptrace lane: evidence for the skip value; E2E skips where ptrace is refused
- deny translation moves to the seam: -1 -> -ret_errno before the ops
- test: dump the evidence tail when the deny E2E fails
- test: as-ops goes standalone -- no engine, no interposition
- ... and 5 more (see git history)

## [2.91.0] — 2026-09-12

**The developer persona trail (PR #823): the fifth**

audience card on the site -- debugging, the root persona --
with the replay pin flow, the cookbook's debugging set
surfaced on the docs page, and the recipe count stated
honestly at 41.

- the developer persona returns: the root trail, back on the map

## [2.90.0] — 2026-09-12

**The qemu cross-arch recipe (PR #821): arm64 samples**

detonate on x64 farms via qemu-user and the aarch64 preload,
with the kernel-lane caveat spelled out -- verified live.

- qemu cross-arch detonation: the arm64-on-x64-farm recipe, verified

## [2.89.0] — 2026-09-12

**The reference compose stack (PR #819): otelcol,**

retraced, and a supervised detonation in one docker compose
up -- denials live in the collector, chained in the journal,
CI-validated on every PR.

- supervisor-compose: the whole story, one docker compose up

## [2.88.0] — 2026-09-12

**The sanitizer compatibility matrix (PR #817):**

retrace's fault injection on sanitizer-instrumented targets --
supported on gcc-13+/clang with the runtime-first incantation,
categorically fatal on macOS dyld, hanging on gcc-11 -- with
an integration tripwire that proves every cell.

- sanitizer matrix: the hang boundary is the TOOLCHAIN -- gcc-11, both arches
- sanitizer matrix: the arm64/gcc-11 cell hangs -- a verdict, not a mystery
- sanitizer matrix: gcc names its runtime libasan -- and a directory is not a file
- sanitizer matrix: the Linux cell's incantation -- runtime first
- sanitizer matrix: the fuzzing persona's combo, supported where it holds

## [2.87.0] — 2026-09-11

**Journal signing (PR #814): --signing-key seals every**

graceful close with ed25519 over the chain's head and line
count, and retrace-ctl verify-journal walks the chain and
checks the seal -- trust anchors for external auditors. Also
the replay E2E's thread-race fix (PR #815): the determinism
claim is asserted on a main-thread seam.

- journal signing: the Windows link and the honest skip
- journal signing: the seal -- trust anchors for external auditors
- replay E2E: a main-thread seam -- the logger's own mallocs race the seed stream

## [2.86.0] — 2026-09-09

**The replay slice (PR #812): RETRACE_REPLAY_OUT**

records the resolved seed and every synthesized outcome;
RETRACE_REPLAY_IN forces the seed back and verifies each
outcome -- the 3am failure reproduces at 9am, seedlessly, and
a tampered record names its drift.

- replay slice: the 3am failure reproduces at 9am, seedlessly

## [2.85.0] — 2026-09-09

**Docker2inside (PR #810): a docker save tarball**

becomes the inside.json declared set -- layers in manifest
order, AUFS whiteouts and opaque dirs, PAX long names -- and
the whole capture/grading/hardening rail applies to containers
unchanged.

- docker2inside: the image IS the declared set -- containers join the audit

## [2.84.0] — 2026-09-09

**Evidence redaction (PR #808): patterns of tokens that**

never leave the process -- one transform at the evidence
fan-out covers stdout, files, OTLP, and the journal, on both
OSes. Also repairs the retraced load_policy error formats that
CodeQL gated.

- test: keep the redaction markers out of failure prints -- CodeQL reads tests too
- retraced: repair the load_policy error formats -- CodeQL's two highs
- evidence redaction: secrets never leave the process

## [2.83.0] — 2026-09-08

**The drift read verb (PR #806): retrace-ctl drift**

answers the two-layer verdict over the control plane -- per
session, the libc agents, the observers' seats, the kernel
observation totals, and the live delta. One X-macro row;
dispatch, scope, usage, and conformance all derive.

- ctl drift: the two-layer verdict as a read verb

## [2.82.0] — 2026-09-08

**The supervisor policy templates (PR #803) -- the arc's**

last open card: four ready-to-push holds under
share/policy-templates/ with a push-smoke guard, plus the
daemon now unlinking its ctl socket at exit (a stale file
connected CLIs to a dead inode). Also retries the checkpatch
downloads (PR #804) so a 429 is not a style verdict.

- ci: retry the checkpatch downloads -- a 429 is not a style verdict
- policy templates: the holds the playbooks reference, ready to push

## [2.81.0] — 2026-09-07

**The quickstart on the launch arm (PR #801): the**

canonical tour is now every control-plane verb -- policy at
boot, spawn for the specimen, kill with the departure
journaled, and the complete bundle on graceful close.

- quickstart rides the launch arm: every step a control-plane verb

## [2.80.0] — 2026-09-07

**The ctl feed seam (PR #799): bytes, lines, and verbs**

are one module's pipeline -- the connection's framing state
moves into the ctx behind retraced_ctl_feed, main.c keeps the
transport, and the framing contract (split lines, batched
commands, partial silence, oversized refusal) is finally
testable.

- ctl feed: the byte layer joins its module -- the seam, cut through

## [2.79.0] — 2026-09-07

**The website catch-up to v2.78 (PR #797): the feed,**

badge, and about card now carry the launch arm and its audit
trail -- spawn, the reap doctrine, the verb table, the
recursive session tree, and mmap.

- website catches up to v2.78: the launch arm and its audit trail

## [2.78.0] — 2026-09-07

**The reap doctrine (PR #795): spawned workloads'**

departures are journal records -- a SIGCHLD self-pipe routes
every child death to the poll loop, which reaps and records
retrace.ctl.exit, with retrace.ctl.* joining the journal's
durable classes so an exit is visible without an unrelated
flush.

- ctl exit statuses: the reap doctrine -- departures are journal records

## [2.77.0] — 2026-09-06

**The ctl verb table (PR #793): one X-macro list is the**

SSOT the daemon dispatch, the scope gate, and the CLI usage all
derive from -- a verb is one list line plus one handler. Also
fixes the events reply truncating at 64 bytes (a dead sizing
variable, found by the extraction).

- ctl verbs: the X-macro table -- one list, every surface derives

## [2.76.0] — 2026-09-06

**Ctl spawn (PR #791): retrace-ctl spawn forks workloads**

armed to join the daemon -- supervisor env, nonce, EAGER connect,
caller-chosen preload -- with the launch journaled before the
child can act and the child taking a full (never spectator) seat.

- ctl spawn: launch workloads that join the daemon themselves

## [2.75.0] — 2026-09-06

**V2.75.0: the sessions tree walker goes recursive --**

depth stops lying at level 4, with the binding path driven
for real and the nesting asserted by parsing the reply.

- sessions: the tree walker goes recursive -- depth stops lying at level 4

## [2.74.0] — 2026-09-03

**V2.74.0: mmap/munmap interception -- the prototypes**

PR #414 asked for in 2019, shipped on today's rails. Fault
injection reaches the memory level: fail_first on mmap is
deterministic OOM at the page, below every allocator.

- mmap/munmap: the prototypes PR #414 asked for, on today's rails
- website catches up to v2.73: the evidence plane leads

## [2.73.0] — 2026-09-03

**V2.73.0: retrace-ctl events -- the evidence read arm**

over the control plane. The journal's tail, chain verdict
riding the reply; evidence pulled over a network carries its
own integrity statement.

- retrace-ctl events: the evidence read arm over the control plane

## [2.72.0] — 2026-09-03

**V2.72.0: retrace-ctl sessions -- the session tree as**

a tree (per token, nested by parent, spectators marked), plus
two CI root fixes: the qemu leg skips emulated perf benches,
and the noderetrace gate treats probe trouble as a skip.

- noderetrace gate: probe trouble is a skip, never a failure
- alpine qemu leg: the perf benches measure the emulator
- retrace-ctl sessions: the tree the registry always carried

## [2.71.0] — 2026-09-02

**V2.71.0: fail_first -- the transient-fault action, the**

retry-path primitive. The first N invocations return the
chosen value with the real call never made; every call after
runs for real. Fault injection can finally verify resilient
code, not just break it.

- fail_first: the transient-fault action -- the retry-path primitive

## [2.70.0] — 2026-09-02

**V2.70.0: noderetrace -- the Node runtime agent, the**

lane's third adapter. Three independent implementations,
three hook systems, one protocol: the conformance claim's
strongest evidence, and the runtime lane covers the three
runtimes that dominate dynamic deployments.

- noderetrace: the Node runtime agent -- the lane's third adapter

## [2.69.0] — 2026-09-01

**V2.69.0: agent.c becomes two files. One TU per**

platform, selected by CMake like tools/retraced selects its
transports -- no behavior change, every platform edit starts
in the right file, and the preprocessor dead zones that hid
type errors from the compiling host are gone.

- agent.c becomes two files: one TU per platform

## [2.68.0] — 2026-09-01

**Publish: the rpm glob (the artifacts/-prefixed set)**

The v2.68.0 rpms built, validated, and reached the artifact
store -- the publish step's glob list carries the artifacts/
prefix and missed the rpm lines the upload set gained. Both
lists are separate surfaces; carry the shape in each.

- rpm artifacts join the release: the hosted runners can build them after all
- docs and site catch up to v2.67: the packaging story

## [2.67.0] — 2026-09-01

**Alpine release leg rides the packaging module**

The container job still hand-staged lib/include-only tarballs
-- the shape the matrix jobs shed in v2.67.0 -- so a musl
user's download carried no bin/: no retraced, no converters,
on the platform whose static-linking story needs them most.
cpack -G TGZ, same as every other leg; the hand-staging block
deletes outright.

- release: the publish globs carry the packaging module's new artifacts
- CPack becomes the packaging module; the artifacts finally carry the tools
- one pipe harness: the integration tests stop carrying five folk copies

## [2.66.0] — 2026-08-31

**V2.66.0: one ring. The agent's twin event queues**

become a pure caller-locked module -- the Windows drop-count
drift heals, the POSIX peek-overwrite race closes, and the
queue gains its first unit surface.

- one ring: the agent's twin queues become a pure module

## [2.65.0] — 2026-08-31

**V2.65.0: one frame codec. The Windows agent's frame**

send/recv adopt the shared retrace_rpc_frame_encode/decode
with the POSIX agent's 2048-byte cap -- closing the silent
loss path where an oversized Windows event was dropped whole
by the daemon's receiver. The conformance suite now pins both
agent halves to one codec.

- one frame codec: the Windows agent adopts the module it already had

## [2.64.0] — 2026-08-31

**V2.64.0: one policy ladder. POLICY_SET validation**

moves to policy_sig.c as retrace_policy_validate -- shared by
both agent halves, with the install staying where the process
state lives. The Windows copy's drift heals: expired policies
are refused there too. Six ladder cases join the policy_sig
unit test.

- policy_sig: time.h for the ladder's expiry guard
- one policy ladder: validation moves in with its verifier

## [2.63.0] — 2026-08-31

**V2.63.0: the broadcast seam -- policy crosses the**

pipe. retraced_ctl_push_policy sends through an installed
conn_send sink over transport-opaque conn handles, the Windows
daemon registers its pipe agents into the control plane, and
policy_push reaches Windows agents for the first time. The
codebase's last extern-as-interface retires; the broadcast is
unit-testable through a fake sink (full peer pushed, spectator
skipped).

- the broadcast seam: policy crosses the pipe

## [2.62.0] — 2026-08-31

**The Windows activation arc lands**

The pipe daemon, the SCM service lifecycle, the named-pipe transports, and the everywhere-set build conformance: a retraced that builds and runs natively on Windows, with every agent and gate symmetric across both worlds.

- 23 follow-up commits in this release; see the git history for the arc.

## [2.61.0] — 2026-08-30

**Jretrace crosses to the pipe (PR #752) and the conformance gate**

follows -- both reference runtime agents and the third-party
acceptance suite now run on POSIX and Windows alike. Minor bump:
new platform capability.

- jretrace crosses to the pipe; the conformance gate follows

## [2.60.0] — 2026-08-30

**Pyretrace pipe-native (PR #750) completes the runtime lane's**

Windows symmetry, and the docs interface catches up to five
releases of shipped capability. Minor bump: new platform
capability.

- pyretrace goes pipe-native + the docs catch up to v2.59

## [2.59.0] — 2026-08-30

**The kernel-observation agents speak the named pipe natively on**

Windows (PR #748) and sign-policy works for the Windows fleet. Minor
bump: new platform capability.

- kernel-observation agents go pipe-native on Windows + sign-policy follows

## [2.58.0] — 2026-08-30

**Retrace-ctl on Windows (PR #746): the fleet CLI over the ctl named**

pipe -- the roundtrip seam's second adapter, the whole command
surface on both OSes, Windows E2E green on the runners. Minor bump:
new platform capability.

- retrace-ctl on Windows: the fleet CLI over the ctl pipe

## [2.57.0] — 2026-08-29

**Policy key rotation and the Windows agent heap fallback (PR #744):**

multi-key pinning lets old and new verification keys overlap during
rotation, and oversize agent events ride the heap instead of dropping.
Minor bump: new capability.

- policy key rotation + the Win agent heap fallback (review A+B)

## [2.56.0] — 2026-08-29

**Retraced as a Windows service (PR #742): the SCM lifecycle --**

StartServiceCtrlDispatcher with console fallback, stop requests
routing to the graceful shutdown path, flags on the binPath, the
real sc create/start/stop/delete E2E on elevated runners. With it,
TODO.supervisor is complete: daemon, sessions, policy epochs,
conformance, fleet CLI, TLS transport, playbooks, pipes, ACLs, and
now the service. Minor bump: new platform capability.

- retraced as a Windows service: the SCM lifecycle (supervisor/12 P1)

## [2.55.0] — 2026-08-29

**The hang-incident hardening (PR #740): poll-set hygiene (listener**

POLLNVAL = journaled fatal exit, connection POLLNVAL = drop), a
10,000-iteration spin backstop, and --exit-after self-termination on
both daemons, wired into the fd-activation test. A daemon that cannot
make progress now dies loudly, and an orphaned test daemon can never
outlive its harness. Minor bump: new hardening capability.

- retraced: full spin guards + --exit-after (the hang-incident hardening)

## [2.54.0] — 2026-08-29

**The daemon-seam deepening (PR #738): one frame state machine behind**

both transports, drift counting on the registry entry (fixing the
Windows count/summary split), the shared policy loader seam, and the
POSIX POLICY_ACK epoch update carried to Windows. Minor bump:
architecture deepening + a behavioral fix on Windows.

- deepen the daemon seam: one frame state machine + one policy loader

## [2.53.0] — 2026-08-28

**Pipe ACL hardening (supervisor/12 P1, PR #736): explicit owner-only**

DACLs on both named pipes -- token owner, Administrators, SYSTEM, and
no world grant; the E2E inspects the DACL and fails on any world
identity. Minor bump: hardening of a shipped capability.

- retraced pipes: explicit owner-only DACLs (supervisor/12 P1 hardening)

## [2.52.0] — 2026-08-28

**Signed policies (supervisor/05, PR #734): Ed25519-wrapped POLICY_SET**

-- the signature covers the exact blob bytes, agents verify against a
pinned RETRACE_SUPERVISOR_PUBKEY and refuse invalid or partial
wrappers fail-closed, and retrace-ctl sign-policy emits wrappers for
policy authors. Minor bump: new capability, ABI unchanged.

- signed policies: Ed25519-wrapped POLICY_SET, fail-closed on a pinned key (supervisor/05)

## [2.51.0] — 2026-08-28

**The supervised loop completes on Windows: the in-process agent speaks**

the supervisor protocol over the named pipe (PR #732) -- HELLO with
the env nonce seats a full peer, POLICY_SET applies again, heartbeats
and the bounded final drain + BYE mirror the POSIX agent, with the
same fail-open liveness and counted-loss discipline. Minor bump: new
platform capability, ABI unchanged.

- agent on Windows: the pipe half of the supervised loop (supervisor/12 P0)

## [2.50.0] — 2026-08-28

**The named-pipe transport lands: retraced on Windows**

(supervisor/12 P0) -- the same RTRD protocol, registry, journal,
nonce/spectator discipline, ctl surface, and live drift grading over
\\.\pipe\, with the POSIX loop untouched. Minor bump: new platform
capability, ABI unchanged.

- retraced on Windows: the named-pipe transport (TODO.supervisor/12 P0)
- docs + website: the supervisor arc is the product, not the roadmap

## [2.49.0] — 2026-08-28

**The beyond-libc completion release: P1 wave (sandbox-exec + dual-path,**

live drift grading, TLS fleet + scopes, jretrace), P2 wave (audited
artifacts, ETW agent, socket activation + privilege drop, agent guide),
and follow-ups (Ed25519 audit signatures, the Windows AppContainer
backend). Also repairs the version.h STRING drift (2.39.0) so release
artifacts finally carry the tag's version.

- 16 follow-up commits in this release; see the git history for the arc.

## [2.48.0] — 2026-08-27

**Kernel-observation agent: the eBPF lane goes live (TODO.beyond-libc/03)**

retrace-ebpf-agent: a supervisor-protocol agent (the reference-
stub skeleton -- no retrace headers, no C linkage) feeding the
journal kernel-source events. Doctrine: kernel agents are
OBSERVERS -- the agent HELLOs WITHOUT the nonce deliberately, so
the daemon seats it as a spectator: evidence ALWAYS, policy
NEVER (a seated-as-full warning fires if a future daemon drifts).

Sources:
- --synthetic: demonstration events on an interval (no BPF
  privileges; the CI path and the protocol proof)
- --loader: wraps the ebpf-bridge loader, converting its retrace
  JSON stdout into kernel-source events (root/CAP_BPF hosts)

integration-ebpf-agent E2E: 3 kernel observations journaled,
spectator seat asserted, zero policy reach asserted -- the
libc-lane and kernel-lane now share one hash-chained journal.


## [2.47.0] — 2026-08-27

**Pyretrace: the Python runtime agent (TODO.beyond-libc/04)**

A third-party implementation of the supervisor protocol on the
reference-stub skeleton (no retrace headers, no C linkage):
sys.audit hooks give the runtime's own syscall-ish boundary,
attributed to the layer a libc interposer sees only as 'an open
from pid N'.

- pyretrace.supervise(): joins RETRACE_SUPERVISOR env (sock +
  nonce); a no-op when absent (the preload plane's gating
  doctrine). Registers as source=runtime, a FULL peer when
  nonce'd -- the journal shows runtime-attributed events in the
  same session.
- audit coverage: file reads/writes (with path), socket
  creation, subprocess/os.system exec; pyretrace.emit() for
  explicit runtime events.
- integration-pyretrace E2E: HELLO/WELCOME(full), a python
  open() journaled as py.file.read, socket() as
  py.socket.create, a direct emit -- all asserted from the
  journal after a graceful daemon stop (the durability
  contract).

- kernel enforcement compile (TODO.beyond-libc/01): retrace-profile enforce + retrace-enforce
- conformance: the reference stub agent (TODO.supervisor/11 P0)

## [2.46.0] — 2026-08-27

**Perf: respect the engine-entry law in the folded lookup**

The first cut resolved the prototype inside the entry lookup --
but the self-heal's func_get logs through log_dbg, whose
formatter dispatches malloc/snprintf; before the reentrance guard
that recursion is unbounded (the Linux CI stack overflow: every
preloaded target SIGSEGV'd). The entry now resolves only the
SLOT (hash + probe + real, all member-law calls); the dispatch
tail, post guard, reads the prototype from the same slot and runs
the self-heal there. Same one-probe win (19.5 -> 10.6 ns/op),
entry law intact. Also: the slot lookup guards a NULL real_out
(the proto-only wrapper crashed the bench).

- perf: one probe, two answers -- the folded name lookup (1.8x)
- sandbox: compiled path sets -- membership in one bucket, not a walk
- agent: the stack formatter takes the emit path's const kv type
- retraced: the ctl plane as a module (the command surface, unit-tested)

## [2.45.0] — 2026-08-27

**Evidence pipeline: zero-alloc emit + loss signaling; policy acks are...**

The emit path paid 7 mallocs + 7 frees per event (two jesc per
attribute) exactly when the target was busiest. The queue slots
now own inline storage: the common event formats straight from
the stack into the slot -- zero allocations -- and the escaping/
oversize cases fall back to the heap path (jesc) unchanged. The
formatter is exported and unit-pinned (decline contract).

Loss signaling: queue drops were counted but invisible -- a hole
in audit evidence nobody saw. Each drain now reports the drop
delta as a retrace.agent.dropped event (the slots freed by the
drain make room for the marker), so the journal records its own
gaps -- the same doctrine as the journal's unclean marker.

One contract fix surfaced by the local suite (masked on CI by
timing): POLICY_ACK records carry no name field, so the journal
writer's buffering classed them as routine telemetry -- a
refusal ack sat in the stdio buffer for the whole audit window.
Policy decisions (applied or refused) are control-plane records:
the durable classes now match the ack shape too.

- evidence pipeline: zero-alloc emit + loss signaling; policy acks are durable
- journal: open once, flush at durability points, record the gaps
- supervisor playbooks P0 (TODO.supervisor/09)

## [2.44.0] — 2026-08-27

**Freeze: the quiet hold -- pure timeouts pass through**

A wildcard freeze fabricated returns for EVERY intercepted call,
including sleep()/usleep(): a frozen polling loop never slept, spun
at full CPU, and every spin iteration was another dispatch -- the
hold AMPLIFIED the load it was meant to stop (found running the
cookbook-39 flow; the recipe had to document freeze-then-kill-fast
as a workaround).

Pure timeouts are inert by definition: passing them through keeps
the specimen quiet while everything else stays frozen.
nanosleep/clock_nanosleep never reach an action (no prototype ->
call real), so only the two wrapped time calls need naming.

Unit tests: fabricated returns per type (ptr -> NULL, int -> -1) and
the exemption for sleep/usleep.

- tests: the wrong-uid probe accepts both refusal flavors
- tests: retrace daemon E2E accepts the auth-record attribution
- tests: expect the auth journal event; skip uid probe without sudo
- retraced: fix pollfd/slot mismatch after a disconnect
- control-plane transport auth P0 (TODO.supervisor/08)
- perf: the dispatch tail's prototype lookup joins the name cache -- 143x
- ... and 2 more (see git history)

## [2.43.0] — 2026-08-26

**Perf release: real-impl cache (132x dispatch resolve), hashed**

config-cache index, agent kick fast path, daemon per-agent RSS
fix, MSVC cache-key port.

- perf: MSVC port -- volatile fallback for the cache key + stdint
- perf: 69x dispatch resolve + hashed config cache + kick fast path + daemon RSS

## [2.42.0] — 2026-08-26

**Agent: kick is one-shot -- atfork registration once per image**

The ubuntu-22.04 (glibc 2.35) and Alpine (musl) session hang: fork's
prepare handler locks g_agent.mu through the INTERPOSED
pthread_mutex_lock, which runs a full engine dispatch whose kick
re-registered via __register_atfork -- blocking on the atfork_lock
that the very same fork() holds. Self-deadlock at every target fork.
Newer glibc dedups duplicate handlers and hid it; 2.35 and musl do
not (musl instead deadlocks relocking g_agent.mu across duplicate
handlers in one prepare pass).

The one-shot flag registers exactly once per image; fork children
inherit the registration and spawn their agent through emit's
pid-ownership reset, as before. Verified: 4/4 supervisor E2Es pass;
diag loop on ubuntu-22.04 under load.

- retrace-ctl: add the binary and its E2E
- otlp: fleet labels (TODO.supervisor/06) -- session_id + agent_id on every span
- v2.43.0: retrace-ctl -- the fleet CLI (TODO.supervisor/07, P0)

## [2.41.0] — 2026-08-26

**Sessions and trees (plan 04): the session token, the fork half-agent, and the tree E2E**

The daemon mints a 128-bit session token at first HELLO; agents stamp it into the environment so children inherit the session; tokenless children link to their parent and the registry carries the tree. Fork children reset their agent state (close the inherited socket, drain, re-HELLO under the child's pid). The E2E walks the full detonation tree: root, fork children, env-scrubbed re-links, exec holes.

- 55 follow-up commits in this release; see the git history for the arc.

## [2.39.0] — 2026-08-25

**retraced slice 3: the in-process control agent**
(TODO.supervisor/03). `RETRACE_SUPERVISOR=1` (plus optional
`RETRACE_SUPERVISOR_SOCK`) arms an agent inside the traced
process: the THIRD permanent-guard background thread, connecting
to the retraced daemon over the plan-01 protocol.

- `src/supervisor/agent.{h,c}`: HELLO at attach (the daemon
  mints the id), HEARTBEATs carrying the event sequence,
  complete EVENT payloads per the protocol schema, PING
  responder, jittered backoff 0.5s→30s on daemon loss.
- **All the Wave B/C laws**: enqueue-only producers (bounded
  256-slot queue, drop-with-count, never blocks a denied call),
  one CAS spawner (lazy on first event, never the constructor),
  logging-off-then-guard ordering on the thread, fail-open
  liveness (daemon absent changes nothing for the target),
  bounded 2s deinit flush so short-lived targets still deliver.
- The sandbox's `deny()` fans the denial out to both event
  subscribers: the OTLP live streamer (Wave C) and now the
  agent — symmetric calls at one site, no logger text-sniffing.
- Unarmed by default: byte-for-byte the old library when the
  env is unset (98/98 ctest unchanged — the zero-delta gate).
- E2E `integration-supervisor-agent`: phase 1 denial lands in
  the daemon journal with full schema + minted attribution;
  phase 2 kills the daemon mid-flight and proves the target
  unchanged.

## [2.38.0] — 2026-08-25

**retraced arc, slice 1: the control protocol**
(TODO.supervisor/01 + 11). The formally-defined wire protocol
between agents, the retraced daemon, and controllers.

- `src/supervisor/protocol.{h,c}`: RTRD framing (magic +
  version + type + 1 MiB-capped length, little-endian) and the
  frozen v1 message table (10 messages: HELLO/HEARTBEAT/
  POLICY_ACK/EVENT/RING_DATA/BYE agent→daemon; WELCOME/
  POLICY_SET/CMD/PING daemon→agent) as an X-macro — the single
  source of truth.
- **SSOT enforced by construction**: the conformance suite
  (test/conformance/) PARSES the header and derives the JSON
  Schemas (share/rpc-schema/*.json) and byte-exact wire goldens
  (golden/*.bin); the C frame test asserts the C encoder is
  byte-equal to the same goldens. Two implementations, one
  artifact set; drift in either direction is a red test
  (proven by sabotage: a renamed or re-id'd message fails CI).
- Receiver rules tested C-side: truncation at every header
  offset, wrong magic, oversize-length rejection without
  allocation, unknown-type forward-skip (compatibility rule).
- No engine linkage: the protocol module is a standalone
  static library consumed by the daemon, agent, and ctl (the
  next slices).

## [2.37.0] — 2026-08-25

**The architecture-deepening release.** Four refactors that turn
shallow modules into deep ones (each judged by the deletion
test; report on file):

- **One-line unit-test registration**
  (`retrace_add_unit_test`): the 40×20-line boilerplate in
  `test/unit/CMakeLists.txt` collapsed from 1,862 to ~700
  lines. The next cross-cutting link/flag change is one edit,
  not forty; the next test is one line.
- **Function-inventory conformance** (new
  `unit-test-inventory-conformance`): every backend's
  `funcs_symbols.S` is preprocessed with the build's own
  platform defines and diffed against the shared
  `funcs_symbols.def`. The v2.36.0 incident class (linux-aarch64
  silently missing 28 functions incl. the socket family) is now
  a red CI check — proven by replaying the v2.35.0 drift
  against the test.
- **The otlp allocator is its own module**
  (`src/core/otlp_allocator.{c,h}`): the size-header shim
  (the musl heap-overflow lesson) extracted from otlp_live with
  its own 7-case unit suite; the supervisor-agent arc reuses it
  as-is.
- **A log-sink seam in the logger**
  (`retrace_log_sink_register`): the flusher's emit path no
  longer hard-codes OTLP; feature modules subscribe (the live
  streamer is the first, the planned supervisor agent the
  next). Verified zero-delta through the live-streaming
  integration tests.
- Website What's-New unfrozen: the v2.7–v2.37 arc (OTLP waves,
  static-CRT Windows, ETW, fuzz workbench, jail) documented.

## [2.36.0] — 2026-08-24

**otlp-c Wave C: security events** (TODO.trace-profile/32) —
the security workflows stop being file-only; findings stream
into whatever OTLP pipeline the team already runs.

- **Live jail denials**: a `sandbox` action denial emits an
  OTLP LOG record (`retrace.jail.denied`, severity ERROR) the
  moment it happens — policy violations visible in
  Grafana/Datadog during detonations, not after. Same bounded,
  never-blocking contract as the span pipeline (Wave B).
- `retrace-fuzz-report --endpoint URL`: crash/assertion
  clusters as LOG records (`retrace.fuzz.*`), campaign counters
  as METRICS, and a WARN LOG when the drift oracle trips.
- `retrace-profile export` now also emits kernel-grading gauges
  (`retrace.risk.{agreed,libc_only,kernel_only}`) when the
  profile was saved with `--kernel` — sub-libc access counts
  per graded binary over time.
- **Documented attribute schema** in `docs/reports.md`
  (`retrace.jail.*`, `retrace.fuzz.*`, `retrace.drift.*`,
  `retrace.risk.*`) — dashboards can be shared against stable
  names.
- Base-URL fix (Wave B follow-up): `RETRACE_OTLP_ENDPOINT` now
  appends `/v1/traces` when no path is given — a bare
  `http://collector:4318` previously posted spans to `/`
  (otelcol 404). Logs/metrics always routed correctly.
- At-exit stats line gains `logs_emitted=` / `logs_sent=`.
- New integration test `integration-otlp-jail` (jailed run →
  assert `/v1/logs` + `/v1/traces` POSTs); unit coverage for
  the event API; fuzz-report/profiler exports verified
  end-to-end against the fixture collector.

## [2.35.0] — 2026-08-24

**otlp-c Wave B: live streaming from the traced process**
(TODO.trace-profile/31) — `RETRACE_OTLP_ENDPOINT=URL` makes
retrace emit OTLP spans LIVE as the wrapped calls happen.

- `src/core/otlp_live.{h,c}`: a new core module that owns the
  otlp-c exporter, a tracer, and a background thread that pumps
  `otlp_exporter_tick()`. Per-call hook path: logger emit → MPSC
  enqueue → exporter tick → POST to collector.
- **Permanent reentrance guard** (`retrance_guard_enter_permanent`):
  the otlp-c background thread and the log flusher both hold it
  for their lifetimes, so their own libc calls (send, connect,
  malloc) pass through to the real impl — no self-interposition
  recursion. Same lesson as the TODO 28 NtWriteFile fix.
- otlp-c's allocator is wired to `retrace_real_impls.{malloc,free}`
  with a thin `malloc+memcpy+free` realloc shim, so the library's
  internal slab/MPSC/span allocations never reach the engine.
- New `retrance_guard_enter_permanent` API + a permanent sentinel
  (`RETRANCE_GUARD_PERMANENT = (void *)0x1`); unit-tested in
  `test_reentrance_guard.c` (8 tests passing).
- **At-exit diagnostics** (`retrace: otlp_live: emitted=… sent=…
  dropped_full=… dropped_err=…`): stderr line at process teardown
  so users see what happened.
- **Integration test** (`test/integration/test_otlp_live.py` +
  `test/fixtures/fixture_otlp_server.py`): end-to-end check with
  a stub OTLP/HTTP collector — verified locally with 623 spans
  emitted, 623 sent, 0 drops.
- otlp-c now built at the top level (canonical `add_subdirectory`);
  `tools/otlp-converter/CMakeLists.txt` uses `if(NOT TARGET otlp_c)`
  guard so the call is idempotent.

## [2.34.0] — 2026-08-24

**otlp-c Wave A: real OTLP export** (TODO.trace-profile/30).

- `otlp-c` vendored (`third_party/otlp-c`, BSD-3-Clause, pinned
  v0.6.13-2-gdefcd7a; pure C99, zero non-libc deps; see
  `THIRD_PARTY_NOTICES` and `third_party/README.md`).
- `retrace-to-otlp --endpoint URL`: posts real OTLP **protobuf**
  spans to a collector (otelcol/Tempo/Jaeger/Honeycomb); the
  OTLP/JSON stdout mode stays for pipes and inspection.
- `retrace-profile export <profile.json> --endpoint URL`: pushes
  per-function timing stats as OTLP gauges
  (`retrace.call_{p99,max,total}_us`) + call counters
  (`retrace.call_count`), tagged `retrace.func` — honest
  aggregates only, no fabricated distributions. Profile files
  now round-trip timings (`from_json` restores them).

## [2.33.0] — 2026-08-24

**Grammar fuzzing: `@`-template dict lines** (TODO.trace-
profile/29) — plus the docs pass.

- `fuzz_str` dictionaries: a line starting with `@` is a
  TEMPLATE expanded at load; `%N%` (1-9) substitutes the Nth
  flat token in file order. Templates reference only flat
  tokens — no nesting, cycles impossible by construction;
  out-of-range references fail the load loudly. Flat-dict
  behavior unchanged. Two fixtures + two tests.
- Quickstart: OpenBSD/NetBSD runner (`ktrace`/`kdump` kernel
  truth) — all five kernel-truth platforms now have a runnable
  quickstart.
- Cookbook recipe 35 (dictionary fuzzing) joins the index.

## [2.32.0] — 2026-08-24

**Grammar fuzzing: `@`-template dict lines** (TODO.trace-
profile/29) — plus the docs pass.

- `fuzz_str` dictionaries: a line starting with `@` is a
  TEMPLATE expanded at load; `%N%` (1-9) substitutes the Nth
  flat token in file order. Templates reference only flat
  tokens — no nesting, cycles impossible by construction;
  out-of-range references fail the load loudly. Flat-dict
  behavior unchanged. Two fixtures + two tests.
- Quickstart: OpenBSD/NetBSD runner (`ktrace`/`kdump` kernel
  truth) — all five kernel-truth platforms runnable.
- Cookbook recipe 35 (dictionary fuzzing) joins the index.

## [2.31.2] — 2026-08-24

**Diag: hook-install success dump** (TODO.trace-profile/28
evidence round).

- `RETRACE_WIN_DIAG=1` now also prints every *successfully*
  installed hook's accepted prologue length and the raw target
  bytes (via ODS, captured by the VEH payload machinery). The
  `NtCreateFile` correctness defect (hooked call fails, no
  crash) becomes visible as data: what the disassembler
  accepted vs. the stub's real shape.

## [2.31.1] — 2026-08-24

**VEH fault-site breadcrumb** (TODO.trace-profile/27 follow-up;
diagnostic infrastructure for the open ntdll+injection crash).

- `RETRACE_WIN_DIAG=1` now installs a vectored exception handler
  at DLL attach: on any exception it WriteFile-logs the
  exception code, faulting address, containing module + offset,
  and the raw bytes at the fault site — hand-disassembler fuel.
  Pure Win32 + static buffers (no CRT on the path), recursion
  latched, observes only (`EXCEPTION_CONTINUE_SEARCH`). The
  next CI run of the static-binary smoke names the faulting
  module of the open crash without any debugger.

## [2.31.0] — 2026-08-24

**Static-CRT Windows binaries: observed and jailed through the
ntdll layer** (TODO.trace-profile/27 — the last platform
deferral, rewritten rather than punted).

- A /MT binary carries its own CRT — no ucrtbase to hook. The
  CI smoke (a true static-CRT target, `MSVC_RUNTIME_LIBRARY
  MultiThreaded`) proves: static binaries launch and run
  cleanly under injection; their configs parse (after the CRLF
  fix); and the ntdll hooks fire inside the /MT process during
  boot.
- FOUND AND FIXED (library bugs the proof surfaced): (1)
  `conf_init` read configs in text mode — every CRLF config
  (i.e. every config written by Windows tooling) silently fell
  back to the wildcard default ("fread failed, errno: 0"
  against a valid file); now binary mode. (2) `retrace-win-run`
  returned 0 on successful launch, discarding the child's exit
  code and masking crashes; it now exits WITH the child's code.
- FOUND AND FIXED (crash): `RETRACE_WIN_NTDLL=1` crashed any
  target under injection. A one-round CI bisect (each of the 8
  opt-in hooks enabled alone, `RETRACE_WIN_NTDLL_LIST`) named
  `NtWriteFile`: the logger's own `fprintf` write re-entered
  the engine through the hook (unlisted functions get the
  default log script), recursing to stack death — an AV that
  cannot dispatch through any handler, which is why the VEH
  never saw it. Fix: the write-path hooks (`NtWriteFile`,
  `NtReadFile`) are removed from the opt-in set (content-level
  rw truth belongs to ETW/procmon; path truth stays). The
  bisect env ships as debug tooling.
- FOUND AND OPEN (correctness): the `NtCreateFile` trampoline
  breaks the hooked call's success path on current images
  (bisect: `NTDLL_LIST=NtCreateFile` alone → the target's fopen
  fails, exit 3, no crash). Relocated-prologue issue; tracked
  in TODO.trace-profile/28 with the hook-set modernization.
- Honest docs (`docs/platforms.md`): CRT-level argument mutation
  is impossible for static CRTs; syscall-boundary observation is
  the right layer (crash bug above notwithstanding).

## [2.30.0] — 2026-08-23

**Converter-main DRY** (TODO.trace-profile/26).

- The five `*2retrace` converter CLIs (strace, dtrace, truss,
  ktrace, etw) were structural ~90-line clones. They now share
  one driver — `tools/common/converter.c` (`converter_main`) —
  and each CLI wrapper is a ~25-line table: name, usage text,
  `convert()` hook, row noun. A sixth converter is a convert
  function plus a table entry, no new CLI code (OCP).
- Behavior-preserving: all five tool outputs byte-identical on
  their inputs (compared pre/post refactor); CLI, exit codes,
  and the stderr count line unchanged; golden tests untouched
  and green (89/89).

## [2.29.2] — 2026-08-23

**Fixed: `etw2retrace` numeric Id table corrected from the OS's
own manifest.**

- The v2.27.0 provisional mapping mislabeled named read/write
  events (manifest truth: **14 = Close, 15 = Read, 16 = Write**
  — the CI manifest print caught it). No wrong rows were emitted
  in CI flows, but real captures with named Read/Write events
  would have mislabeled.
- Full table now transcribed from `Get-WinEvent -ListProvider
  Microsoft-Windows-Kernel-File` (machine truth, printed by the
  CI smoke every run): `NameCreate→open`, `NameDelete→unlink`,
  `Create→open`, `Read→read`, `Write→write`, `SetDelete→unlink`,
  `Rename→rename`, `QueryInformation→stat`, `DeletePath→unlink`,
  `RenamePath→rename`, `CreateNewFile→open`. Close/Cleanup/
  SetInformation/DirEnum/Flush/FSCTL/OperationEnd stay skipped
  (no name or no POSIX-shape semantics).
- Fixture + golden extended (Id10/Id14/Id15/Id16/Id18 rows).

## [2.29.1] — 2026-08-23

**Polish.**

- `memory_fuzz`: missing-params error message named the wrong
  action (copy-paste from `modify_return_value_int`).
- `etw-capture.ps1`: the event-count line printed 1 regardless
  of the real row count (PowerShell unroll bug).
- ETW CI smoke now prints the OS's own Kernel-File Id↔task
  manifest table each run — machine truth for completing the
  converter's numeric Id mapping (the Id10 follow-up).

## [2.29.0] — 2026-08-23

**Packaging audit completes: `personal-files`/`system-files`
plugs mapped** (TODO.trace-profile/19 follow-up).

- `retrace-snap2inside` now reads the top-level `plugs:` section
  of snapcraft.yaml: plugs with `interface: personal-files` or
  `system-files` map their author-declared `read:`/`write:`
  path lists into accesses (`$HOME` expands to the concrete
  home, `SNAP2INSIDE_HOME` override as before). Previously
  these plugs were reported unmapped.
- Honest scope, stated in the output docs: this maps the snap's
  REQUEST; what snapd actually connects is admin policy outside
  snapcraft.yaml. `raw-usb` & co. stay unmapped notes.
- Golden fixture extended (personal + system plug, $HOME
  expansion, write class); 89/89 tests.

## [2.28.0] — 2026-08-23

**Dictionary-driven string fuzzing — `fuzz_str`**
(TODO.trace-profile/25; the content-fuzzing deferral closes).

- New action: replaces an incoming `sz` param with a token drawn
  from an AFL-style dictionary file (one token per line, `#`
  comments and blanks skipped; 256 tokens x 4096 bytes bounded).
  Deterministic under the shared seed machinery — reproducer =
  config + seed + dict. Optional `match_str` gates the
  replacement to calls carrying a given value.
- Seed policy extracted (`fuzz_seed_init`): a script using only
  `fuzz_str` still seeds (param > `RETRACE_FUZZ_SEED` > time).
- Loader split into `fuzz_dict.{c,h}` — unit-testable without
  action machinery; 12 new tests (load semantics, pick
  determinism, action param validation, replacement, match
  gating, no-leak repeated calls). 89/89 total.
- `examples/fuzz-workbench`: `paths.c` demo target + dictionary
  section in the runner — three runs, token sequence extracted
  from the trace logs, byte-compared (the reproducibility
  promise, shown not claimed).
- `docs/configuration.md`: full `fuzz_str` reference.

## [2.27.0] — 2026-08-23

**Scripted Windows ETW kernel truth — the last kernel-truth gap
closes** (TODO.trace-profile/24).

- `scripts/win/etw-capture.ps1`: logman trace session on the
  Microsoft-Windows-Kernel-File provider around the target
  (admin required), Get-WinEvent extraction of named file events
  to a pinned-shape raw jsonl, PID-scoped to the target.
- `retrace-etw2retrace`: raw jsonl -> retrace trace JSON. Task
  names normalize to the POSIX-shaped names the correlate
  classifier knows (`CreateFile` -> `open`, `DeleteFile` ->
  `unlink`, ...). Nameless rows (CloseFile carries a FileObject,
  not a name) and unmapped tasks are skipped.
- Golden-file test over an 8-row fixture covering every
  skip/mapping class; CI E2E smoke on every Windows leg (`cmd /c
  type hosts` through capture + convert, asserting the read).
- `docs/platforms.md` and the quickstart Windows runner: ETW is
  the scripted path; procmon stays the zero-install fallback.

## [2.26.0] — 2026-08-23

**TODO 07 closed — the Windows env "mystery" was never about
env** (TODO.trace-profile/23).

- Three CI evidence rounds, no library changes needed: (1) both
  env views — Win32 `GetEnvironmentVariableA` and CRT `getenv` —
  agree on all four `RETRACE_LOGGER_*` vars before boot; (2) the
  log file holds ~4 KB of real entries before deinit — hooks,
  engine dispatch, and the logger all work on MSVC uncrutched;
  (3) the failure was the test's own reader: `FILE_SHARE_READ`-
  only open vs the logger's deliberately never-closed append
  handle = `ERROR_SHARING_VIOLATION`, so a healthy log READ as
  empty.
- The historical `RETRACE_WIN_DIAG=1` ctest forcing ("empirically
  load-bearing") is removed. Every MSVC and MinGW leg passes
  without it.
- Test-side only: share-tolerant `read_all`, a `log-state` probe
  breadcrumb, investigation scaffolding stripped.

## [2.22.0] — 2026-08-22

**Fuzz-workbench completion** (TODO.trace-profile/20).

- **Drift oracle** (`--baseline <profile.json>`): clean fuzz
  iterations aggregate into one profile and diff against the
  baseline — behavior the baseline never saw (a new path, a new
  function) surfaces in the report EVEN WHEN NOTHING CRASHED.
  Verified E2E: a corpus binary adding an undeclared fopen
  reports `fopen` + `/etc/hosts` as drift against the baseline.
- **Minimized corpus** (`--emit-corpus <dir>`): one reproducer
  per failure cluster — the reproducing set, ready to keep.
- fuzz-workbench example demonstrates all three phases (crash
  clustering + reproducer replay, drift oracle, minimization).

## [2.21.0] — 2026-08-22

**The fuzzing workbench** (TODO.trace-profile/20, Wave 3).

- **`retrace-fuzz-report`**: run a corpus of seeds against a
  target under a fuzz config; classify each iteration (crash /
  assertion via --marker / clean); cluster failures by
  (last-called function, param count); emit report.json plus
  ONE REPRODUCER per cluster -- the same config + the failing
  RETRACE_FUZZ_SEED. Exit 1 on crash clusters (CI-able).
  Verified E2E: unchecked-malloc target, 8/8 crashes clustered,
  reproducer replays its crash.
- **`RETRACE_FUZZ_SEED` env**: memory_fuzz now accepts an
  external seed -- ANY fuzz config becomes deterministically
  re-drivable without editing it (the reproducibility path).
  Explicit fuzz_seed action_params still win.
- **Truncated-trace tolerance in clustering**: crashes kill the
  logger mid-write; the tolerant scanner (stream.c) attributes
  from complete entries; a death before any entry flushed is
  its own honest "?" cluster, never merged with a named
  function. Cluster ids/seeds serialize as strings (> 2^53
  JSON-double precision).
- **examples/fuzz-workbench**: the runnable demo (crashy.c,
  8-seed corpus, verification that a reproducer replays).
- 7 new unit tests (clustering semantics + JSON shape).

## [2.20.0] — 2026-08-22

**Packaging & container audit** (TODO.trace-profile/19, Wave 2):
claims-vs-truth moved up to the packaging layer.

- **`retrace-snap2inside`**: snapcraft.yaml app plugs -> the
  declared-set format; observed behavior graded with
  `--inside` reports accesses outside the granted interfaces as
  DECLARED-SET VIOLATIONS. The home interface maps to the
  concrete home; unmapped plugs (personal-files etc.) are noted,
  never dropped silently.
- **`retrace-flatpak2inside`**: flatpak JSON manifest
  finish-args (filesystem=host/home/path:ro|rw, share=network,
  device=all) -> the declared set; unmapped args noted; the
  YAML manifest form is honestly refused in v1.
- **`retrace-profile harden`**: the jail exported as
  infrastructure -- a docker-compose fragment from the profile
  (read_only, cap_drop ALL, no-new-privileges, rw/ro binds by
  class, network off when none observed, env whitelist).
- **Declared-set grading**: `retrace-profile --libc --inside`
  now prints the violations headline; `--libc` accepts profile
  docs like diff/jail.
- **examples/packaging-audit**: the runnable flow (declared ->
  observed -> violations -> jail -> compose), verified E2E.

## [2.19.0] — 2026-08-22

**Jail depth & deception** (TODO.trace-profile/18, Wave 1 of the
security-research arc).

- **Read-only detonation** (`--read-only` / `deny_classes:
  ["write"]`): any write-class call dies regardless of path --
  class from function + mode/flags (fopen "w/a/+", open
  O_WRONLY/O_RDWR low bits, unlink/rename/mkdir/... name set).
  Runtime-proven: a write to an ALLOWED path is denied.
- **Deception mode** (`--decoy <dir>` / `decoy_dir`): instead of
  denying an undeclared READ, the path is rewritten to
  decoy_dir/<basename> and the real call runs against the decoy.
  A denial is a detectable signal; a plausible fake keeps the
  sample on its happy path. Every redirect is logged
  (sandbox: DECOYED). Runtime-proven: the app reads the decoy
  unknowingly.
- **Env jail** (`allow_env`/`deny_env`): first-class env NAME
  policy -- denied getenv returns NULL, denied setenv -1.
  Runtime-proven.
- **Clock pinning** (`--pin-clock <epoch>`): appends a time()
  script pinned to a fixed epoch for deterministic reruns
  (drift oracles, diffable traces). v1 pins coarse time only.

## [2.18.0] — 2026-08-22

**Documentation, examples, and a real jail fix** (TODO.trace-profile/
15-17).

- **Fixed: denied pointer calls returned -1, not NULL** — the
  sandbox's deny path synthesized ret_val = -1 for every denial;
  callers of pointer-returning functions got `(FILE *)-1`,
  passed the `!= NULL` check, and crashed using it. Now
  prototype-driven (`deny_ret`): `ptr`-returning functions deny
  with NULL, others with -1 (POSIX open-family error). One root
  cause behind both the macOS quickstart segfault and the
  "jail x getenv" crash.
- **`retrace-profile diff` accepts profile docs** — inputs may
  now be traces OR profile docs (the artifact the upgrade story
  actually hands around).
- **docs/reports.md**: every output shape annotated — trace
  entry, profile doc, risk report, drift report (human +
  --json), jail config, validate output.
- **docs/platforms.md**: one honest per-platform guide — capture,
  kernel truth per OS, jail, env vars, and the limitations of
  each platform.
- **examples/trace-profile-quickstart**: the whole loop
  (capture -> validate -> diff -> jail -> denied run) as one
  runnable artifact per platform — Linux, macOS (+dtruss),
  FreeBSD (+truss), Windows (VS prompt .bat).

## [2.17.0] — 2026-08-22

**Honest-gap closure round 2** (TODO.trace-profile/12, 14).

- **macOS kernel truth** (14): `retrace-dtrace2retrace` converts
  `dtruss` captures to the trace format — the claims-vs-truth
  grading (`--kernel`, `SUBLIBC_ACCESS_FOUND`) now runs on
  macOS (dtrace needs SIP off). dtruss ` ` string suffixes
  stripped; syscall names normalized to POSIX
  (`open_nocancel` → `open`).
- **FreeBSD kernel truth** (14): `retrace-truss2retrace` for
  `truss -f` captures.
- **arm64 hook decoder** (12): the v1 stub copied 16 prologue
  bytes blindly — an ADRP in the window made the trampoline
  compute a wrong page and the first arm64 hook pass-through
  segfaulted. Real allowlist decoder now: pair loads/stores,
  add/sub immediates, mov/orr forms, register-base loads,
  pointer-auth hints, nop — anything PC-relative (adrp/adr,
  branches, literals) or unknown REFUSES the hook. The x86-only
  thunk follower is compiled out on arm64.
- **Jail denial proven on Windows** (12): the wrapper round-trip
  test now runs under a sandbox allowlist — an undeclared path
  is denied (no usable FILE*) while declared paths execute.

## [2.16.0] — 2026-08-21

**Honest-gap closure round 1** (TODO.trace-profile/11, 13, 12a):
Windows profiles stop reporting false negatives.

- **Env/net visibility on Windows** (11): `getenv` (ucrt) and
  `connect`/`send`/`recv` (ws2_32) are now hooked default-on.
  Before, the capture config listed them but no hook existed --
  profiles silently reported `env: []` and `net: []`.
- **ntdll data ops** (13): `NtWriteFile`, `NtReadFile`,
  `NtQueryDirectoryFile` join the opt-in ntdll set -- a
  Win32-direct `WriteFile`/`ReadFile`/`FindFirstFile` program is
  no longer invisible past its opens. Prototypes in
  `src/core/prototypes/ntdll.c`; the write/read classification
  lands by function name.
- **23 wrappers** per dialect (was 16): all four
  arch/toolchain twins extended in lockstep, name table
  index-ordered.
- **arm64 runtime un-gate** (12a): the wrapper round-trip test
  runs on the MSVC-arm64 legs -- the armasm64 dialect gets its
  first runtime evidence (bounded TIMEOUT as everywhere).

## [2.15.0] — 2026-08-21

**Trace + profile, completed** (TODO.trace-profile/07-10): the
loop closes on Windows, and the upgrade story gets its last
step.

- **`retrace-profile capture` on Windows** — no preload on
  Windows, so capture delegates the launch to `retrace-win-run`
  (found next to the profiler, with `retrace.dll`); the
  profile/jail emission path is the shared portable code. The
  recipe-34 flow is now one command on every platform
  (examples/profile-hunting/run-windows.md).
- **`retrace-profile jail <profile.json>`** — emit a jail config
  from an existing profile doc (or trace): the update-the-jail
  step of the upgrade story (profile old → upgrade → profile
  new → diff → jail), no re-capture. `--inside` supplies the
  declared allowlist; without it the observed accesses self-jail
  a known-good run. Jail emission extracted to
  `tools/profiler/jail.c` (model logic out of the CLI).
- **armasm64 wrapper dialect** (`wrapper_arm64.asm`) — live
  ucrt/ntdll hooks on the MSVC-arm64 legs; CMake wires
  `ASM_MARM64`. The gas arm64 twin now uses the label-free
  immediate-index design like the x64 twins (label addressing is
  where assemblers disagree).
- **Fixed: profile aggregation corruption** — `access_add`
  credited a repeat access to `items[lo]` after a bsearch break
  when the match was at `items[mid]`: any profile with two or
  more paths mis-credited hits and classes to the wrong row
  (since 2.12.0). Found by the new jail round-trip tests
  (`test/unit/test_profile_jail.c`, 5 tests: jail shape,
  declared allowlist, to_json/from_json round trip, doc-vs-trace
  jail parity, degenerate inputs).
- **Fixed: Windows direct-hook trampoline loop** — the x64
  trampoline's tail jump computed back to the patched entry
  (`target+0`) instead of past the patched window
  (`target+prologue_len`): prologue replay → jump into the hook
  patch → wrapper → pass-through → trampoline → forever. On
  MSVC's ucrtbase `_read` is a direct export (not a thunk), so
  the first config read during boot looped millions of times —
  the "MSVC action-path hang" since v2.13. Thunk-path functions
  (fopen) use the separate correct builder, which is why MinGW
  never showed it. Caught by the opt-in `RETRACE_WIN_DIAG`
  counter (13,042,527 entries, all `read`); the arm64 twin
  always had the right formula.
- **Fixed: pointer arguments truncated on Windows (LLP64)** —
  `struct FuncParam.val`, `ThreadContext.ret_val`, and the
  `retrace_as_call_real*` / `retrace_as_set_ret_val` signatures
  were `long`, which is 32-bit on Windows: every pointer
  argument was truncated before `log_params` dereferenced it
  (and before `call_real` re-dispatched it), crashing the
  action path on MSVC while LP64 POSIX was unaffected. All
  widened to `intptr_t` (identity on Linux/macOS/BSD).
- **Fixed: Windows thunk hooks never uninstalled** —
  `retrace_win_install_thunk` discarded its hook handle, so
  `retrace_win_uninstall_hooks` silently skipped every
  thunk-followed hook (fopen, _unlink): the 14-byte patch stayed
  live and the next call looped forever through the wrapper
  (fallback real-impl resolution returns the patched export
  itself). New `retrace_hook_bookmark` captures the original
  bytes so uninstall restores them (since v2.13).
- **Windows wrapper test un-gated on MSVC** (TODO.trace-profile/
  07): the log-content assertion runs strictly on MinGW and as a
  documented WARN on MSVC (harness-level env-propagation open
  question in TODO 07), with a bounded TIMEOUT so a trampoline
  loop fails fast instead of hanging the job.

## [2.14.0] — 2026-08-20

**The trace + profile workstream** (TODO.trace-profile): the
recipe-34 loop is now one command, upgrade-aware, and
contract-checked.

- **`retrace-profile capture`** — one-shot: run a command under
  the preload with the right logger env, trace it, reduce to a
  profile, optionally emit the jail. The built-in default config
  scopes tracing to the file/env/net function set (a user
  RETRACE_JSON_CONFIG always wins); a wildcard default traced
  printf-family variadics -- noisy and fragile. POSIX; Windows
  uses retrace-win-run (examples/profile-hunting/run-windows.md).
- **`retrace-profile diff`** — drift between two profiles: new
  paths, class escalations (read → write is the headline),
  removed paths, new functions. Human report + --json; exit 1
  on drift (CI-able). The upgrade story for the tailor loop.
- **`retrace-profile validate` + `share/profile-schema.json`** —
  the profile contract, machine-checkable: enum values, required
  sections, and the cross-field rule a schema cannot express
  (risk present iff the kernel layer was captured).
- **Windows ucrt hook set expanded** (fopen-only before):
  `_open _close _read _write _lseek _stat _unlink _remove
  _rename _rmdir`, mapped to the POSIX-shaped prototypes via the
  new export→engine name field in the hook table.
- **Windows arm64 wrapper** (gas dialect; TODO.trace-profile/05):
  AAPCS64 frame with x30 preserved for the tail-jump; dual-arch
  arch_spec. The armasm64 variant for MSVC-arm64 is the
  documented follow-up.
- **MSVC logger fix**: real_impls never resolved `atoi`, so
  logger_init called a NULL pointer on any env override -- the
  long-gated logger-format tests now run on MSVC.

Bumps: version.h 2.13.0 -> 2.14.0, retrace_cli.c banner,
nix/debian/fedora packaging, CHANGELOG.md.

## [2.13.0] — 2026-08-20

**Windows: the first live hooks.** (TODO.windows/05-06.) The
engine has built on Windows since 2.11.0; now calls actually
flow through it: inline hook -> assembly wrapper ->
WrapperWinX64Frame -> engine -> trampoline back to the real
function.

- **PE-section registry** — actions, prototypes, and data types
  register on Windows (.rtrA/.rtrF/.rtrD sections found via the
  module's own PE headers; PE gives no __start_ symbols and MSVC
  has no constructors). Previously the registries walked empty.
- **Hook layers** — ucrt `fopen` default-on; the ntdll set
  (`NtCreateFile`, `NtOpenFile`, `NtQueryAttributesFile`,
  `NtClose`, `LdrLoadDll`) strictly opt-in via
  `RETRACE_WIN_NTDLL=1` (AV/EDR sensitivity documented). Paths
  decode from OBJECT_ATTRIBUTES/UNICODE_STRING to UTF-8, so
  profile/correlate consume them like any path. libsass-style
  Win32-direct importers are visible at this depth.
- **One injectable `retrace.dll`** — engine + hook core +
  backend in a single DLL (the backend DLL previously shipped
  without the engine). DllMain installs hooks BEFORE boot so
  hooked names resolve to their trampolines, never the patched
  bytes.
- **`retrace-win-run`** — the Windows launcher:
  CreateProcess(SUSPENDED) -> inject -> hooks+boot in the child
  -> resume. One shared injection implementation
  (win_common/inject.c).
- **Windows-arm64** — engine + registry run (tested); the
  arm64 wrapper is the follow-up slice.
- **Docs** — docs/windows.md (injection, hook layers, jail,
  procmon kernel truth), cookbook 34 + tools.md updates.

Bumps: version.h 2.12.0 -> 2.13.0, retrace_cli.c banner,
nix/debian/fedora packaging, CHANGELOG.md.

## [2.12.0] — 2026-08-20

**Profiles: see what a binary does, then jail it to that.**
(TODO.windows/08.) The correlation arc (2.7.0–2.11.2) detected
escapes; this release closes the loop with enforcement.

- **`retrace-profile`** — the claims-vs-truth risk profiler.
  Reduces any trace to a profile (functions, filesystem accesses
  by class, env vars, network addresses). With a kernel-layer
  truth stream (`--kernel`), grades every access by layer
  provenance: kernel-only accesses are the sub-libc surface a
  libc capture can never see (verdict `SUBLIBC_ACCESS_FOUND`).
  A static capability scan (`--binary`) counts raw
  `syscall`/`svc` instruction gadgets in executable segments and
  PE ntdll imports. `--jail-out` emits a ready-to-run
  deny-by-default jail config; the allowlist comes from the
  declared set (`--inside`), never the observed trace — a
  self-allowlisted jail would allowlist its own escapes.
- **`retrace-strace2retrace`** — `strace -f -e trace=%file`
  logs → the common trace format, so the Linux kernel layer
  feeds the profiler (and correlate) like any other stream.
- **`sandbox` `allow_paths`** — deny-by-default mode. Fixes the
  action's param selection (prototype metadata: string params
  only — `close(fd)` integers were previously compared as
  strings) and fails closed (`ret_val = -1`, `errno = EACCES`)
  on misuse. Prefix entries accept `/` and `\`.
- **`log_params` stamps `func`** into every param entry, so
  offline tools attribute calls without parsing banner text.
- **Offline tools on Windows** — `retrace-correlate`,
  `retrace-procmon2retrace`, `retrace-strace2retrace`, and
  `retrace-profile` are portable C and now build on Windows
  (previously the whole `tools/` tree was POSIX-gated).
- **Docs** — cookbook recipe 34 (profile → tailor → jail,
  cross-platform capture matrix), `examples/profile-hunting/`
  runnable end-to-end demo, tools.md / configuration.md /
  architecture.md updates.

- **Correlation coverage criteria** (TODO.windows/01-03) — three
  correctness/ergonomics upgrades to the escape join:
  - **pid scoping.** A procmon capture is system-wide; the
    procmon2retrace → correlate chain now scopes by process:
    `--pid N` on both tools, and coverage is ALWAYS pid-aware
    (an inside record from pid A never covers a touch by pid B;
    pid-less entries stay wildcards).
  - **Time-window covering** (`--window SECONDS`). Pure
    set-difference is time-blind: a materialize logged after an
    open covers it, hiding touches the VFS cleaned up after
    instead of serving in time. With a window, covered = the
    inside stream saw the path within ±SECONDS of the touch.
    Semantics correction caught by writing the unit test first:
    the original TODO claimed set semantics produce phantom
    escapes — backwards; the window is the stricter mode.
  - **Probe/read/write classification.** libsass's importer
    storms ~14 GetFileAttributesW existence probes per @import;
    a probe is an information leak, not a data access. Every
    report line now carries `class=probe|read|write|none`
    (func-name tables + Detail heuristics, pinned by tests) and
    `--exclude-probes` drops probe-class hits (jail-grant
    policies that grant read-attributes wholesale).
- Model: the matcher's inside set became an index of
  {path, pid, time} records (sorted by path+time, binary-search
  coverage probe); the decision takes one criteria value object
  (`struct CorrCriteria` — new criteria extend it, never fork
  the code path); one string-walker feeds both the index and the
  per-entry decision (OCP sink).
- retrace-procmon2retrace `--pid N` — scope system-wide CSVs at
  conversion time.
- Golden contract grows optional per-case `options.txt`;
  report-line format now `escape <path> func=<f> tid=<t>
  pid=<p> class=<c>`. Four new cases: 07-pid-scope (decoy
  process), 08-time-window (lazy materialize discrimination),
  09/10 probe policy on/off; 06-libsass gains a decoy-pid row
  and runs with --pid.

### Tests
- 13-test matcher suite (classify, index, pid/window/probe
  semantics); 75/75 overall (10 golden correlation cases).

## [2.11.2] - 2026-08-20

### Fixed
- **fopen interception was silently bypassed on macOS for every
  target compiled with `_DARWIN_C_SOURCE`** — which includes
  everything built by this project's CMake (it adds the define
  to all Darwin targets). Modern macOS SDKs remap `fopen` to
  `fopen$DARWIN_EXTSN` under that define in optimized builds,
  and the interposition table had no entry for the variant —
  calls went straight to libc, unlogged. Notably, the CI's own
  test/file binary was affected: the macOS legs had zero real
  fopen coverage until now.
  - The Mach-O backends (arm64 + the shared x86_64 table)
    interpose `fopen$DARWIN_EXTSN`.
  - The engine strips the `$DARWIN_EXTSN` suffix at the single
    normalization point (`strip_darwin_extsn`), so prototype
    lookup, config scripts, real-impl resolution, and log
    output all use the clean name — user configs need no
    changes.
  - Verified end to end: a `_DARWIN_C_SOURCE` -O3 binary's
    EXTSN call is interposed, logged as plain `fopen` with the
    dereferenced `*filename` param, and `call_real` executes
    (the CI file test's 2 fopen calls now appear in traces).

## [2.11.1] - 2026-08-20

### Fixed
- **Ring-logger entries were lost when the traced process
  exited immediately after its last calls.** The v2.11.0
  late-call gate cleared `g_logger_ring_ready` at the top of
  `retrace_logger_deinit` -- BEFORE the guards that used it --
  so the flusher's final drain and the ring teardown were dead
  code. Every ring-buffered entry after the first was dropped
  at exit (instant-exit programs lost nearly their whole
  trace). Found by the new escape-hunting demo; bisected
  v2.10.0 (good) vs v2.11.0 (bad) via worktree builds; pinned
  by a new logger-fmt scenario (`ring`) that leaves the ring
  enabled and asserts entries survive an immediate exit.

### Added
- **examples/escape-hunting** (TODO.windows/04 examples
  parity): the recipe-33 flow as a runnable demo -- a packaged
  app reading from a virtualized prefix, an inside.json VFS
  stream, run-posix.sh (retrace capture -> retrace-correlate;
  prints the hidden.dat escape, exit 1) and run-windows.md
  (procmon capture -> retrace-procmon2retrace --pid ->
  correlate). Portable open()-based demo binary; documented
  that current macOS SDKs remap fopen to fopen$DARWIN_EXTSN in
  optimized builds, which the interposition table does not
  export (recorded in TODO.windows/05).
- **Examples build on Windows**: the portable set (getenv,
  escape-demo) compiles on the Windows legs; the socket-based
  demos and root-check carry documented platform notes. The
  examples CMakeLists had never been parsed in the CMake era
  (flag defaulted OFF) -- its root-relative paths are fixed.

## [2.11.0] - 2026-08-19

### Added
- **The v2 core engine builds and links on Windows**
  (TODO.windows/04, Track B slice 1 — the blocker for Windows
  interception since the beginning: RETRACE_BUILD_V2 was
  disabled there because the core assumed POSIX).
  - `src/core/posix_compat.h` — the ONLY place platform
    thread/symbol APIs are touched: rc_mutex_t
    (pthread_mutex | SRWLOCK), destructor-capable rc_tss_t
    (pthread_key | FlsAlloc), rc_thread_* (pthread |
    CreateThread/WaitForSingleObject), rc_getpid/
    rc_thread_self_tid, rc_dladdr (Windows: module base+path
    via RtlPcToFileHeader; symbol matching degrades to module
    matching), rc_backtrace (CaptureStackBackTrace). The
    RetraceRealImpls fields are retyped to the rc_* names —
    the reentrancy guard now holds on both platforms.
  - PE section macros (`win_common/arch_spec_macros.h`):
    registry variables land in one short `.retrc` section
    (PE names cap at 8 chars, no start/stop symbols); the
    walkers report empty registries this slice — item 05
    (first wrapper) wires role-aware registration.
  - `win_common/arch_spec_stub.c`: the retrace_as_*
    trampoline contract links as no-ops until item 05.
  - sockaddr_inspect on winsock2 (sa_family_t/sockaddr_un
    compile shims; AF_UNIX branch is POSIX-runtime-only);
    windows.h macro hygiene (undef ERROR/SEVERITY_ERROR/...)
    so the core enums survive the include.
  - CMake: the Windows build now compiles src/config + src/core
    + backends; Windows CI legs run the portable unit set
    (logger format scenarios, trace load) — 79/79 on POSIX
    unchanged, verified behavior-preserving.
  - Local verification: x86_64-w64-mingw32 cross-build of the
    full tree (core + tests + backends DLL) — the cross
    compiler caught four real Windows-side defects before CI
    (RtlPcToFileHeader arity, SEVERITY_ERROR collision, const
    tss signature, unbalanced braces).

## [2.10.0] - 2026-08-19

### Added
- **Streaming JSONL log output** (`RETRACE_LOGGER_FMT=jsonl`,
  default `json` unchanged -- TODO.windows/07). The array
  document opens `[` at init and closes `]` at exit: a crashed
  trace truncates the tail, nothing can tail it live, and there
  is no per-entry framing. JSONL emits one COMPACT object per
  line: crash evidence survives line-by-line, `tail -f | jq -c`
  works mid-run, and the flusher's per-entry framing is the
  only code that changed (the ring hot path is untouched).
  Verified live through the real library's ring+flusher path.
- **One tolerant scanner for every tool.** The correlate
  scanner moved to `tools/common/stream.{c,h}`; new
  `trace_load_file()` yields the same parsed array from array
  documents, JSONL, and truncated tails. retrace-audit and
  retrace-diff switched to it -- format-agnostic input, zero
  downstream changes (verified: audit and diff produce
  identical output on the same trace in both formats; diff of
  array-vs-JSONL of one trace reports no differences).

### Tests
- 3-case logger-format suite (forked per scenario: the logger
  keeps process-global state -- first-entry flag, ring
  readiness), 5-case trace-load suite. 77/77 overall.

## [2.9.0] - 2026-08-19

### Added
- **`retrace-procmon2retrace` — the Windows outer-layer producer.**
  Converts a procmon CSV export (File > Save as CSV) into a
  retrace JSON log so `retrace-correlate` consumes the kernel
  layer's view on Windows like any other outside stream
  (TODO.next-level Phase 3):
  - `tools/procmon2retrace/csv.{c,h}` — tolerant CSV scanner:
    quoted fields with embedded commas and doubled quotes, CRLF
    (between records AND inside quoted Detail fields), UTF-8 BOM,
    per-field truncation guard, dropped-unterminated-quote policy
    (same tolerance as the correlate scanner). Header row maps
    columns case-insensitively; a missing header falls back to
    procmon's canonical order.
  - `tools/procmon2retrace/convert.{c,h}` — one row to one
    retrace-shaped entry: pid numeric, module ETW, Result SUCCESS
    -> INFO / anything else -> WARN, Operation -> func, the
    original wall-clock timestamp preserved in the message (time
    is 0: procmon's clock has no date), NT paths passed through
    for the correlate normalizer.
  - CLI `retrace-procmon2retrace <in.csv> [out.json]`, streaming
    one array document in retrace's emission shape.
- Cookbook recipe 33 gains the "Windows outer layer: procmon CSV"
  section (convert then correlate, end to end).

- libsass hardening (independently verified tebako's libsass
  investigation @ 9bb4ebcc): corr_normalize now also strips the
  '//?/' forward-slash prefix spelling libsass builds before
  flipping separators (src/file.cpp); new golden case
  06-libsass-importer pins the real-world importer shapes end to
  end — QueryOpen (GetFileAttributesW) probe misses as escapes
  (read-attributes leak), \\??\\ covered hits, and the
  wildcard-not-declared escape — with a sync CTest proving the
  case's outside.json regenerates identically from its procmon
  CSV.

### Tests
- 11-case CSV scanner unit suite, 6-case converter suite, a
  round-trip CTest, the libsass golden case + CSV<->JSON sync
  CTest, and 2 new normalizer cases. 71/71 overall.

## [2.7.0] - 2026-08-19

### Added
- **`pid` + `tid` on every log entry.** The log schema grows the
  process/thread identity pair (getpid / gettid and platform
  equivalents), making streams from multi-process, multi-thread
  targets correlatable — two same-second events are no longer
  ambiguous. Verified live: same pid across threads, distinct
  tids per thread.
- **`retrace-correlate` — the escape-report tool.** Joins an
  inside (VFS, e.g. tebako's tfs) stream against an outside
  (retrace) stream and reports host-filesystem touches the VFS
  never saw:
  - `tools/correlate/match.{c,h}` — path extraction from any
    string field at any depth, path normalization (NT forms
    `\??\`, `\\?\`, `\Device\HarddiskVolumeN\` -> DOS
    drive guess, slash unification, component-boundary prefix
    match), the sorted inside-set and the escape decision.
  - `tools/correlate/stream.{c,h}` — tolerant log scanner: one
    JSON array document, leading-comma emission, truncated tail
    (a crashed trace still yields every complete entry), or
    JSONL.
  - CLI exit codes 0/1/2 mirror retrace-diff (clean / escapes /
    usage-IO); `--json` for machine consumers.
- **Golden parity fixtures** (`tools/correlate/golden/`) — five
  language-neutral cases (posix-clean, posix-escape, nt-forms,
  truncated-tail, jsonl-stream) that pin the correlation
  contract. Third-party correlators (tebako's Rust
  implementation) assert the same cases in their CI; ours do via
  a per-case CTest.
- Cookbook recipe 33 ("Detect filesystem escapes from a
  virtualized environment") and the three-layer correlation model
  section in docs/architecture.md, including the layer-honesty
  statement: a libc-boundary capture cannot certify raw-syscall
  absence.

### Tests
- 9-case correlate matcher unit suite + 8-case scanner unit
  suite + 5 golden CTest cases; 66/66 overall.

## [2.6.1] - 2026-08-19

### Added
- **`retrace_config_validate_buffer()`** — public config
  validation (second ADR-0014 slice). Parses a JSON intercept
  config (comment-tolerant, same as the loader) and checks every
  `func_name` against the prototype registry (the literal `*`
  wildcard allowed) and every `action_name` against the action
  registry, catching the typo classes that otherwise surface at
  runtime as silently-missing interceptions. Optional err_buf
  carries a human-readable message ("unknown action 'log_paramz'
  in func 'malloc'"). 8 new contract tests in the surface-guard
  binary (ok/wildcard+comments/unknown action/unknown
  function/malformed JSON/missing arrays/invalid inputs); the
  guard now enforces 9 symbols.

### Fixed
- **`retrace validate <config.json>` actually validates now.**
  It was a stub since the CLI landed ("TODO: parse JSON and
  check action/func names") that only checked the file existed.
  It now reads the file, dlopens the installed library, and
  reports real errors with the offending names. Exit codes: 0
  valid / 1 invalid config / 2 usage-or-IO.

## [2.6.0] - 2026-08-18

MINOR bump per ADR-0006: public surface grows (ADR-0014's
implementation-first path).

### Added
- **`retrace_list_functions()` / `retrace_list_actions()`** —
  public registry introspection: enumerate every interceptable
  libc function (the prototype registry) and every built-in
  action. Same ownership contract as `retrace_list_backends`.
  Implementation walks the `__retrace_funcs` / `__retrace_acts`
  linker-section arrays; no init required. The section-walk
  macro's block-scope externs are hoisted by clang, so each
  listing lives in its own translation unit (mirroring
  funcs.c/actions.c) with a shared inline builder in
  `public_api_internal.h`.
- Surface-guard test extended: the two new symbols join the
  dlsym list, plus contract tests (counts, non-empty names,
  well-known members: malloc/open/write among 322 functions;
  log_params/call_real/memory_fuzz/capture_buffer among 17
  actions).

### Fixed
- **`retrace list-functions` / `list-actions` actually work
  now.** Both were stubs since the CLI landed ("TODO: link
  against retrace_core"; list-actions printed a hardcoded list
  of 9 actions that had drifted from the real 17). Both now
  dlopen the installed library and print the live registry.

## [2.5.4] - 2026-08-18

### Changed
- CI hardening: `timeout-minutes` on every workflow job that
  lacked one (build.yml 60/30, alpine 45/60, msys 30, nix 30,
  checkpatch 15, website 20/15, docker 30 — fuzz, ohos, coverity,
  release already had them). Previously a hung runner (observed
  during the v2.5.3 cycle: two Linux legs stuck on `apt install`)
  waited out GitHub's 6-hour default before anyone could intervene.

## [2.5.3] - 2026-08-18

### Changed
- **Ring logger default capacity 64 -> 1024** per thread. The
  v2.5.1 contention benchmark showed the 64-slot ring engaging
  drop-on-full at sustained rates above the flusher's drain
  cadence (~64K entries/s/thread): a synthetic 1-thread producer
  at ~500K entries/s delivered under 10% of entries. With 1024
  slots (~1M entries/s/thread ceiling at the 1ms cadence, ~32KB
  per logging thread) the same benchmark delivers **97.6%**
  single-threaded (37x fewer drops) and 68% at 8 threads (was
  17%). Accounting remains exact at every thread count
  (delivered + dropped == pushed).

### Added
- `RETRACE_LOGGER_RING_CAP` env var: per-thread ring capacity,
  power of two in [64, 65536]; anything else falls back to the
  default. Documented in `docs/cli.md` and the README env table.
- `test_env_cap_override` in the ring unit tests: valid override
  (mask == cap-1), non-power-of-two and out-of-range fallbacks,
  suite re-pinned to 64 via the env var so the wrap/drop tests
  stay deterministic against any production default.

## [2.5.2] - 2026-08-17

### Added
- `test/property/test_property_policy.c` — 6 property-based
  tests for `policy_rule_matches` (the audit matcher), 2000
  generated iterations each with a fixed seed (reproducible;
  override via RETRACE_PROPERTY_SEED):
  - **determinism**: same (rule, message) answers identically.
  - **AND-loosening**: a matching rule still matches after any
    single predicate is removed — fewer constraints cannot
    un-match.
  - **empty-matches-all**: a predicate-free rule matches every
    generated message.
  - **func_exact soundness**: a match implies the func equals
    the pattern.
  - **path_contains soundness**: a match implies some string
    value contains the substring (non-matches vacuously sound).
  - **env-glob iff**: suffix/prefix/exact shapes match exactly
    the names that end with/start with/ equal the word.

## [2.5.1] - 2026-08-17

### Added
- `test/perf/bench_log_ring_contention.c` — the lock-free ring
  logger's contention benchmark. N producer threads (1/2/4/8)
  push 20K entries each through the real hot path
  (`log_info` -> ring push) while the flusher drains to a
  counting sink; reports aggregate entries/sec, ns/entry,
  delivered and dropped counts per configuration.
  The correctness invariant it enforces: **delivered + dropped
  == pushed** — drop-on-full is the ring's designed backpressure
  (64 slots + 1ms flusher cadence ≈ 64K entries/s/thread
  ceiling), and every drop must be accounted. First run on this
  machine: push-side cost 0.8-1.9µs/entry; sustained synthetic
  producers at 500K-1M entries/s engage drop-on-full heavily
  with zero unaccounted entries. A portable mutex+cond start
  gate stands in for pthread_barrier (absent on macOS).

## [2.5.0] - 2026-08-17

MINOR bump per ADR-0006: the public header surface changed.

### Changed
- **The public API now matches the implementation** (ADR-0014).
  An audit (`nm -g` on the built library) showed that of ~28
  functions declared in `<retrace/retrace.h>`, only the two added
  in v2.4.0 actually existed — even `retrace_version()` had no
  definition. Consumers compiled against the header and failed at
  link time. The never-implemented declarations (engine
  lifecycle, script builder, action params, config parsing,
  introspection, error reporting) are removed; the re-
  introduction path is documented in the ADR and gated on real
  implementations. No working program can regress: the removed
  symbols never existed in any linkable build.

### Added
- `retrace_version()` and `retrace_version_info()` — implemented
  for real in a new `src/core/public_api.c` (previously declared,
  never defined).
- `test/unit/test_public_api.c` — the surface guard: dlsyms every
  function the header declares from the running image and fails
  the build if any is missing. A declared-but-unlinked symbol can
  never ship again. Also pins the version contracts and the
  attach/list-backends behavior smoke.
- `docs/adr/0014-public-api-matches-implementation.md`.

## [2.4.6] - 2026-08-17

### Changed
- `docs/architecture.md`: Logs section now documents the lock-free
  SPSC ring + background flusher (v2.3.0) and
  `RETRACE_LOGGER_RING`. New "The tooling ecosystem" section maps
  the audit and diff module chains (policy→scan→format→pdf_writer;
  normalize→threshold→lcs→stats) and the shared-JSON producer/
  consumer contract. The backends section documents the ptrace
  attach path (`retrace attach` / `retrace_attach_process`) and
  why it bypasses the probe.
- `docs/development.md`: new "Test conventions" (the CHECK-not-
  assert rule with its Alpine war story, standalone tool-module
  tests, per-commit checkpatch) and "Adding a new tool module"
  (the pure-module → thin-CLI → standalone-test pattern).
- `docs/faq.md`: four new answers — attach to a running process,
  CI gating via `retrace-diff` exit codes, SARIF → GitHub Code
  Scanning, logger overhead. Fixed stale counts (18→27 tutorials,
  21→32 recipes) and the academic-citation version (2.1.0→2.4.5).

## [2.4.5] - 2026-08-16

### Fixed
- `tools/audit-converter/pdf_writer.c` -- two real bugs, both
  present since the PDF output shipped in v2.3.0 and both found
  by the new tests:
  1. **Missing findings page**: the findings-page loop's bound
     (`obj_id < total_objects - 1`) was off by one, so whenever
     the findings content fit exactly (1-40 findings) the page
     was skipped entirely -- the PDF declared `/Count 3` with
     only 2 page objects, and every finding was invisible.
     Beyond 40 findings the last page was dropped. Now the page
     count matches the objects for every case.
  2. **Double-escaped cover text**: the cover pre-escaped the
     policy name and trace path, then `build_page_content`
     escaped them again -- parentheses rendered as literal
     backslash-parens in viewers. The cover now passes raw
     strings and is escaped exactly once (matching the findings
     path).

### Added
- `test/unit/test_pdf.c` -- 13 unit tests: `pdf_escape_string`
  rules (plain/parens/backslash/empty), document structure
  (PDF 1.4 header, %%EOF trailer, xref + startxref,
  Catalog/Pages/Font), page counts for 0/1/40/41 findings,
  cover content, padded summary labels, findings rule ids,
  special-character round trip, and the writer's return value.

## [2.4.4] - 2026-08-16

### Fixed
- `tools/trace-diff` stats mode: switched the variance from the
  one-pass `E[x^2] - mean^2` form to the numerically stable
  two-pass sum-of-squared-deviations. The one-pass form suffers
  catastrophic cancellation for large call counts (values beyond
  2^53 are not exactly representable in a double), producing
  wrong stddevs and therefore wrong z-scores -- found by the new
  large-counts unit test.

### Changed
- `tools/trace-diff`: extracted the z-score math from
  `run_stats_mode` to a new `stats.{c,h}` -- `diff_stats_compute`
  fills mean/stddev/z/no-variance/significance in one call. The
  tool chain is now: `normalize.c` -> `threshold.c` -> `lcs.c` ->
  `stats.c` -> `diff.c` (CLI + printing).

### Added
- `test/unit/test_stats.c` -- 10 unit tests: known distributions
  ([10,12,14] -> 12 +- sqrt(8/3)), significant/not classification,
  negative-direction |z|, zero-variance cases (constant baselines,
  all-zero, single baseline), strict threshold semantics (exact
  z == threshold is NOT significant), custom thresholds, invalid
  inputs, and billion-scale counts.

## [2.4.3] - 2026-08-15

### Changed
- `tools/trace-diff`: extracted the LCS alignment from `diff.c` to
  a new `lcs.{c,h}` -- `diff_lcs_len` (pure length) and
  `diff_lcs_walk` (alignment emitted as typed MATCH/DELETE/INSERT
  items via callback). The tool chain is now: `normalize.c`
  (aggregation) -> `threshold.c` (gating) -> `lcs.c` (order
  alignment) -> `diff.c` (CLI + printing). Behavior-preserving,
  including the both-empty early return before the header.

### Added
- `test/unit/test_lcs.c` -- 17 unit tests: classic LCS lengths
  (identical/disjoint/interleaved/textbook ABCBDAB vs BDCABA),
  empty inputs, prefix cases; walk semantics (all-MATCH for
  identical, only-edits for disjoint, one-side-empty);
  the identities `edits = alen + blen - 2*lcs` and `items =
  alen + blen - lcs`; pointer provenance (MATCH/DELETE names from
  the before sequence, INSERT from after); alignment shape for
  reorderings; early-stop via callback; NULL-callback safety.

## [2.4.2] - 2026-08-14

### Changed
- `tools/audit-converter`: extracted the output formatters from
  `audit.c` to a new `format.{c,h}` -- `audit_sarif_level`,
  `audit_format_default`, `audit_format_sarif`. Completes the
  tool's MECE chain: `policy.c` (rules + matching) -> `scan.c`
  (apply-to-trace) -> `format.c` (render) -> `audit.c` (CLI).

### Added
- `test/unit/test_format.c` -- 11 unit tests. SARIF coverage pins
  everything GitHub Code Scanning keys off: the 2.1.0 skeleton
  (version, $schema, single run, driver name), per-result
  ruleId/level/message.text, severity->level mapping
  (critical/high -> error, medium -> warning, info -> note),
  1-based region.startLine, artifactLocation.uri, and the
  zero-findings empty-results contract. Default-format coverage
  pins policy/trace fields, per-finding evidence (deep-copied
  entry), summary counts per severity, and the zeroed summary.

## [2.4.1] - 2026-08-14

### Changed
- `tools/audit-converter`: extracted the scan engine from `audit.c`
  to a new `scan.{c,h}` — `struct Finding`/`Findings` +
  `audit_findings_init/free/append` + `audit_scan_trace`. MECE
  split completes the module chain: `policy.c` owns rules +
  matching, `scan.c` owns apply-policy-to-trace, `audit.c` owns
  CLI + formatters. The scan engine is now unit-testable in
  isolation (same pattern as the `policy_rule_matches` and
  `diff_exceeds_threshold` extractions).

### Added
- `test/unit/test_scan.c` — 11 unit tests: single rule/entry
  matching, severity preserved through `finding->rule`, findings
  in trace order, policy-rule order within one entry, multiple
  rules per entry, one rule across entries, entries without a
  message skipped, empty trace / zero-rule policy / no-match all
  yield zero findings, `audit_findings_append` growth past the
  initial 16-entry capacity, and the init→free→init lifecycle.
  False negatives here mean missed violations in compliance
  reports; wrong ordering corrupts the evidence chain.

## [2.4.0] - 2026-08-14

MINOR bump per ADR-0006: new capability, backwards-compatible API.

### Added
- **Native process attach — `retrace attach <pid>`**. Attach to an
  already-running process via ptrace and trace its syscalls until
  it exits. No `LD_PRELOAD`, no restart, no control of the launch
  required — reaches the targets the preload backends structurally
  cannot (any running PID; static binaries after they started).
  Output is the same JSON format as `retrace run` and feeds the
  same downstream tools (audit, diff, replay). Linux only; reports
  a clean error elsewhere.
- **`retrace backends`** — lists the interposition backends
  compiled into the library (preload-elf, preload-macho,
  preload-msvc, ptrace, ...).
- **New public API** (`include/retrace/retrace.h`):
  `retrace_attach_process(pid)` — explicit ptrace lookup (bypasses
  the static-binary probe: attach semantics differ from spawn; for
  a process you cannot exec, ptrace is the sole native mechanism
  regardless of how the binary was linked) — and
  `retrace_list_backends(&names, &count)`.
- **`test/unit/test_attach.c`** — 3 tests: backend enumeration,
  invalid-pid rejection, and (on Linux) a real fork + PTRACE_ATTACH
  + timeout-kill round trip verifying the trace loop runs to
  completion. Non-Linux legs verify the clean-failure contract.

### Changed
- `src/backends/ptrace/trace_loop.c`: removed a NULL-engine early
  return that defended an unsatisfiable contract —
  `struct retrace_engine` is never instantiated; syscall dispatch
  goes through the process-global `retrace_engine_wrapper`. The
  handle survives in the signature for the backend API contract.

## [2.3.8] - 2026-08-14

### Added
- Five new tutorials (23–27) covering the v2.3.0 tool ecosystem,
  each following the established Time/Goal/Steps format:
  - [23 — Audit a binary for compliance violations](docs/tutorials.md)
    (`retrace-audit`: baseline policy, SARIF upload, PDF for the
    audit trail).
  - [24 — Catch performance regressions in CI](docs/tutorials.md)
    (`retrace-diff --threshold pct=N` as a GitHub Actions gate).
  - [25 — Debug a captured trace interactively](docs/tutorials.md)
    (`retrace-replay`: forward regex search, backward stepping,
    index jump).
  - [26 — Watch a long-running server's calls live](docs/tutorials.md)
    (`retrace-ws`: browser viewer + programmatic Python client).
  - [27 — Trace a binary `LD_PRELOAD` can't reach](docs/tutorials.md)
    (Frida bridge: static binaries, attach-to-running-PID, focused
    function lists).

### Changed
- `docs/README.md` routing table and doc-map updated: tutorials
  count 22 → 27; "See also" in tutorials links the cookbook
  (32 recipes) and `tools.md`.

## [2.3.7] - 2026-08-14

### Fixed
- `src/config/json/parson.c`: `json_parse_string_with_comments(NULL)`
  no longer segfaults. The public API function called `strlen(NULL)`
  before any NULL check, which would crash any caller that passed
  NULL by accident. Now returns NULL gracefully. Bug found while
  writing the budget regression test.

### Changed
- `src/config/json/parson.c`: removed duplicate `static size_t
  parson_alloc_total;` and `parson_alloc_budget;` declarations.
  Both pairs initialized to 0 (C tentative definition semantics),
  so behavior was unchanged, but the duplication was a clear bad-
  merge artifact.

### Added
- `test/unit/test_parson_budget.c` — 10 unit tests guarding the
  v2.3.0 allocation-budget mechanism against regressions: realistic
  configs parse, comments parse, repeated parses don't accumulate,
  edge cases (NULL, empty, whitespace-only, malformed) return NULL
  gracefully, large configs stay within budget. Includes the
  NULL-input test that caught the segfault bug above.

## [2.3.6] - 2026-08-14

### Changed
- `tools/trace-diff`: extracted `exceeds_threshold` from `diff.c`
  to a new `threshold.{c,h}` as the public `diff_exceeds_threshold`.
  MECE split mirrors the existing `normalize.c` extraction:
  threshold math, trace aggregation, and orchestration/printing now
  live in three separate files. The function is now unit-testable
  in isolation.

### Added
- `test/unit/test_threshold.c` — 15 unit tests covering
  `diff_exceeds_threshold` across every edge case: identical
  values, zero/negative threshold (report-any mode), 0→N unbounded
  growth, N→0 at/below 100% threshold, positive and negative
  direction at/above/below threshold, large-threshold suppression,
  and the strict-`>` boundary (at-threshold does not report).

## [2.3.5] - 2026-08-13

### Added
- `docs/configuration.md`: `capture_buffer` action reference.
  Covers `param_name` (required), `size_param`, `max_bytes`
  (default 4096, hard cap 4096), `format` (`hex` default vs
  `string`), and the non-printable byte replacement behavior.
  Cross-references recipe 22 (`decode_http` / `decode_dns`) for
  the structured-fields alternative.
- `docs/cli.md` + `README.adoc` env var tables: `RETRACE_LOGGER_RING`
  (lock-free ring + background flusher vs synchronous writes) and
  `RETRACE_CALL_HASH` (per-thread FNV-1a coverage hash for
  libFuzzer custom mutators). Both shipped in v2.3.0 but were
  absent from the env var reference until now.

## [2.3.4] - 2026-08-13

### Changed
- `README.adoc`: new "What's new in v2.3.x" section consolidating
  highlights from v2.3.0–v2.3.3 (lock-free logger, capture_buffer,
  call_hash, parson OOM hardening, the entire tools ecosystem,
  fuzz-replay CLI, nightly fuzz workflow, website features). New
  "Tooling ecosystem" section with one row per standalone tool.
  "Supported platforms" header bumped from v2.2.0 to v2.3.3.

### Added
- `docs/cli.md`: `fuzz-replay` subcommand section. The subcommand
  shipped in v2.3.0 but was undocumented in the CLI reference until
  now.

## [2.3.3] - 2026-08-13

### Added
- Nine cookbook recipes for the v2.3.0 tool ecosystem:
  - [24 — Audit a trace for compliance violations](docs/cookbook/24-audit-compliance.md) (`retrace-audit`).
  - [25 — Detect performance regressions between two builds](docs/cookbook/25-diff-regression.md) (`retrace-diff` with `--threshold` and `--stats`).
  - [26 — Diff the call-order between two runs](docs/cookbook/26-diff-call-order.md) (`retrace-diff --order` LCS).
  - [27 — Time-travel replay for a trace](docs/cookbook/27-replay-debug.md) (`retrace-replay`).
  - [28 — Live-stream a trace over WebSocket](docs/cookbook/28-live-stream.md) (`retrace-ws`).
  - [29 — Trace an iOS / static / running process via Frida](docs/cookbook/29-frida-bridge.md).
  - [30 — System-wide file-access tracing with eBPF](docs/cookbook/30-ebpf-system.md).
  - [31 — Browse a trace in VS Code](docs/cookbook/31-vscode-viewer.md).
  - [32 — Visualize a trace in Grafana](docs/cookbook/32-grafana-dashboard.md).
- [`docs/tools.md`](docs/tools.md) — top-level overview of the
  tools ecosystem with a "when to reach for what" table and per-tool
  reference linking back to the cookbook.
- `docs/README.md` updated: new row in the routing table for the
  tools overview; doc-map reflects the larger cookbook count.

### Changed
- `docs/cookbook/README.md` — new "Tooling ecosystem" section
  indexes recipes 24–32 with one-line summaries and direct tool
  references.

## [2.3.2] - 2026-08-13

### Changed
- `test/helpers/test_utils.h`: new `CHECK()` macro for always-on
  post-condition checks. `assert()` compiles to `((void)0)` under
  `-DNDEBUG` (CMake Release default), eliding both the check and
  any side effects in the tested expression. `CHECK()` always
  evaluates; on miss it prints a FAIL line, increments
  `tests_fail`, and returns from the test function.

### Fixed
- 8 unit test files (`test_call_count_limit`, `test_decode_http`,
  `test_decode_dns`, `test_filter`, `test_fuzzing_seed`,
  `test_log_flusher`, `test_log_ring`, `test_modify_return_value_int`)
  had side-effecting function calls wrapped inside `assert()`.
  Under CMake Release builds these calls were silently elided,
  leaving state uninitialized and assertions unverified. Every
  `assert(action(ctx, p) == X)` is now `rc = action(ctx, p);
  CHECK(rc == X);`. The bug class previously caused v2.3.1 RC
  tests to segfault on Alpine/musl + gcc -O3 while passing on
  glibc/macOS by luck.

### Added
- Migrated 7 of the 8 affected test files to `test_utils.h`
  (DRY: shared TEST macro, action_fn_t typedef, JSON builders,
  `init_minimal_real_impls()`, `finish_tests()`). Each file now
  has ~30 fewer lines of duplicated boilerplate.

## [2.3.1] - 2026-08-12

### Changed
- `audit-converter`: moved `rule_matches` from `audit.c` to `policy.c`
  as the public `policy_rule_matches`. Rule-matching semantics now
  live with the rule data model (MECE split); `audit.c` is purely
  trace-scanning + output formatting.

### Added
- `test/unit/test_policy.c` — 25 unit tests for the audit policy
  module (severity_str round-trip, policy_load_from_json variants,
  policy_rule_matches across all 4 predicate types including
  suffix/prefix/exact env_pattern, AND semantics, NULL safety).
- `test/unit/test_normalize.c` — 12 unit tests for the trace-diff
  normalizer (call-count aggregation, duration sum, engine-noise
  skip, ordering preservation, NULL safety, free idempotency).

## [2.3.0] - 2026-08-12

### Added

#### Lock-free logger (TODO 19)

- Per-thread SPSC ring buffer replaces the global-mutex logger
  hot path. Background flusher thread drains rings at 1ms cadence.
  Eliminates contention; sustains 100K+ events/sec on multi-core.
  `RETRACE_LOGGER_RING=0` env gate for platforms where background
  threads are unstable (OHOS Docker/QEMU).

#### capture_buffer action (new — 17th built-in)

- Post-call memory observation. Reads N bytes from a pointer param
  (after `call_real`) and logs as hex or string. Critical for
  security auditing: see WHAT was read/received, not just that
  read()/recv() was called.

#### Call-hash coverage feedback for libFuzzer (TODO 24)

- Per-thread FNV-1a rolling hash of intercepted libc calls.
  Exported as `retrace_call_hash_last` global for a libFuzzer
  custom mutator (`fuzz_call_hash`) that biases mutations toward
  inputs exercising new call sequences.

#### Compliance audit tool (TODO 26)

- `retrace-audit` reads a retrace JSON log, applies policy rules,
  and emits findings as JSON, SARIF 2.1.0, or printable PDF.
  Ships with 4 policies: baseline, PCI-DSS, HIPAA, ISO 27001.

#### Differential trace analysis (TODO 27)

- `retrace-diff` compares two traces per function call-count and
  total-time. Supports `--threshold` (CI gating), `--order` (LCS
  sequence alignment), and `--stats` (statistical significance
  via z-score against N baseline traces).

#### Time-travel replay (TODO 25)

- `retrace-replay` interactive tool: step forward/backward
  through trace events, jump to indices, search by regex.
  Surfaces `capture_buffer` entries alongside parent calls.

#### WebSocket live streaming (TODO 22)

- `retrace-ws` tails the JSON log and broadcasts to WebSocket
  clients. Built-in browser viewer with function-name highlighting
  and severity coloring.

#### Frida bridge (TODO 28)

- `retrace-frida.js`: hooks 30 libc functions via Frida's
  Interceptor, dereferences string args (path, name, command),
  emits retrace-compatible JSON to stdout.

#### eBPF backend (TODO 29)

- BPF program + Python loader skeleton for kernel-level syscall
  observation on Linux. Observation-only (eBPF cannot mutate).

#### VS Code extension (TODO 31)

- `retrace: Open Log Viewer` — webview with function-name
  highlighting and severity coloring. `retrace: Show Stats` —
  per-function call count quick-pick. `retrace: Connect to Live
  Stream` — WebSocket client consuming retrace-ws output.

#### Grafana data source (TODO 32)

- Loads retrace JSON log (via HTTP) as a Grafana time series.
  5-second cache TTL for live-reload dashboards.

#### CLI fuzz-replay (TODO 33)

- `retrace fuzz-replay <fuzzer-name> <crash-input>` replays a
  crash reproducer through the named libFuzzer harness.

#### Nightly fuzz workflow (TODO 33)

- `.github/workflows/fuzz.yml` runs all 5 fuzzers for 5 minutes
  each, uploads crash artifacts (exit 77/71/70) on failure.

#### Website interactive features (TODO 36)

- Decision Wizard: 4-step wizard (goal → sub-goal → target →
  config + command + recipe link).
- Recipe Builder: drag-and-drop action chain composer with live
  JSON output.
- Wasm playground: browser-based trace demo running entirely in
  WebAssembly.

#### Cookbook recipes 22-23

- Recipe 22: Decode HTTP and DNS wire formats.
- Recipe 23: Bridge to OpenTelemetry (OTLP) via retrace-to-otlp.

### Fixed

- macOS dyld destructor crash: `thread_context.c` destructor was
  deleting the global pthread_key from a per-thread context.
- OHOS Docker/QEMU crash: background flusher thread spawned during
  constructor. Fixed via lazy spawn + `RETRACE_LOGGER_RING=0`.
- Alpine arm64 perf-bench timeout: reduced iterations from 100K
  to 10K for QEMU-compatible runtime.
- Parson OOM: 129-byte adversarial input caused ~2GB allocation.
  Added allocation budget (`input_size * 1000`) to
  `json_parse_string_with_comments`.
- CLI `cmd_trace` stack overflow: snprintf return accumulation
  without bounds checking. Fixed via extracted config builders.

### Changed

- Logger hot path: global mutex → per-thread SPSC ring + background
  flusher (3 PRs: ring, flusher, integration).
- Audit tool: MECE refactor — scan/present/format layers are
  separate (OCP: adding a format = new function, no engine change).
- Action param lookup: DRY extraction into `retrace_action_find_param`
  shared across 8 sites in 5 files.
- Flusher stop: `pthread_join` (hangs on macOS dyld destructor) →
  spin-wait + grace period.
- Removed `retrace_logger_log_old` (dead code, pre-JSON legacy).

## [2.2.2] - 2026-08-08

### Added

#### HTTP/1.x protocol decoder (TODO 23 MVP)

- New `decode_http` action that reads a named buffer param
  (typically from `send`/`recv`), parses the first line as
  HTTP/1.x, and logs the decoded fields.

  ```json
  {
    "func_name": "send",
    "actions": [
      { "action_name": "decode_http",
        "action_params": { "param_name": "buf" } },
      { "action_name": "log_params" },
      { "action_name": "call_real" }
    ]
  }
  ```

  Parses both request lines (GET/POST/PUT/DELETE/PATCH/HEAD/
  OPTIONS/CONNECT/TRACE) and response lines (HTTP/1.1 STATUS).
  Non-HTTP data is silently skipped (no-op).

## [2.2.1] - 2026-08-08

### Added

#### Filter action (TODO 20 MVP)

- New `filter` action for conditional guards in intercept scripts.
  Evaluates a single param comparison (`==`, `!=`, `>`, `<`, `>=`,
  `<=`). If false, aborts the script (no logging, no modification,
  no `call_real`). Compose multiple filter actions for AND
  semantics.

  ```json
  {
    "actions": [
      { "action_name": "filter",
        "action_params": { "param_name": "flags", "op": "==", "value": 0 } },
      { "action_name": "log_params" },
      { "action_name": "call_real" }
    ]
  }
  ```

  Eliminates the "log everything, grep later" pattern for the
  common case of param-value filtering.

## [2.2.0] - 2026-08-08

### Added

#### Network function interception (TODO 15)

- 27 BSD-sockets functions now intercepted: `socket`, `connect`, `bind`,
  `listen`, `accept`, `send`, `recv`, `sendto`, `recvfrom`, `setsockopt`,
  `getsockopt`, `socketpair`, `accept4` (Linux/BSD only), `shutdown`,
  `sendmsg`, `recvmsg`, `gethostbyname`, `getaddrinfo`, `freeaddrinfo`,
  `gai_strerror`, `inet_pton`, `inet_ntop`, `inet_addr`, `inet_aton`,
  `inet_network`, `getpeername`, `getsockname`.
- New `addr_deny` action -- network deny-list (the address-space
  counterpart of `sandbox`). Specs support `"host:port"`, `"*:443"`,
  `"[::1]:443"`, `"/var/run/x.sock"`, `"*"` (deny all).
- New `retrace_sockaddr_inspect` helper -- uniform view of `sockaddr*`
  across `AF_INET` / `AF_INET6` / `AF_UNIX` families.

#### Per-return-address routing (TODO 17)

- New `caller_matches` array on each `intercept_script`. Three match
  kinds (OR-semantics):
  - `address` -- exact return address
  - `symbol` -- caller's symbol name via `dladdr`
  - `offset_in_module` -- ASLR-safe module-relative offset
- Per-process dladdr cache (256 entries, mutex-protected) brings
  repeat-lookup cost from ~10us to ~1us.
- Backward-compatible with the existing single-value `return_addr` field.
- New cookbook recipe 17 with three working examples.

#### Property-based test suite (TODO 16)

- P0: parson (3 properties) + `sockaddr_inspect` (8 properties).
- P1: actions (5 properties: `modify_return_value_int`,
  `incomplete_io`, `call_count_limit`, `fuzzing_seed`).
- Engine slice: `script_resolver` (5 properties), `caller_match`
  (5 properties, including P14 acceptance criterion).
- Total: ~26 properties, ~26,000 evaluations per `ctest` run.

#### Engine MECE refactor (TODO 13)

- Five distinct modules, each owning one concern:
  - `thread_context.c` -- per-thread lifecycle
  - `reentrance_guard.c` -- in-use marker
  - `cleanup.c` -- post-intercept reset
  - `script_resolver.c` -- find matching script
  - `action_runner.c` -- dispatch the actions array
- `engine.c` is now pure orchestration.
- New `docs/engine-state-machine.md` documents the 16-state
  per-call lifecycle.

#### Spec coverage (TODO 14)

- Unit tests for all 13 built-in actions: `addr_deny`,
  `modify_return_value_int`, `call_count_limit`, `sandbox`,
  `modify_in_param_int`, `fuzzing_seed`, `delay`, `log_params`,
  `call_real`, `incomplete_io`, `memory_fuzz`, `modify_in_param_str`,
  `modify_in_param_arr`, plus `sockaddr_inspect` helper,
  `caller_match` + `caller_cache`, `reentrance_guard`, JSON config
  loader.

#### Stress test suite (TODO 35 P0)

- `stress_threads` scenario: 8 threads x 100K iters x 4 calls/iter =
  3.2M intercepted calls per family. Labeled `stress`; opt in via
  `ctest -L stress`.

#### libFuzzer harnesses (TODO 33 P0)

- `fuzz_config_parse` -- parson comment-tolerant parser. Smoke run:
  369K iterations, 0 crashes.
- `fuzz_script_resolve` -- script_resolver surface. Smoke run:
  1.99M iterations, 0 crashes.
- Opt in via `-DRETRACE_BUILD_FUZZERS=ON` (clang-only).

#### Performance benchmarks (TODO 34 P0)

- Harness: `bench.h` with `clock_gettime` + percentile reporting.
- 4 benchmarks: `bench_script_resolve`, `bench_caller_match`,
  `bench_action_log_params`, `bench_action_call_real`.
- Labeled `perf`; opt in via `ctest -L perf`.

### Changed

- `script_resolver` now reads `caller_matches` array with OR-semantics
  (takes precedence over legacy `return_addr` single-value field when
  present).
- Test pyramid restructured: `unit/`, `property/`, `stress/`, `fuzz/`,
  `perf/` subdirectories, each with its own CMake label.

### Fixed

- Two latent test bugs caught by Debug-build assertions (previously
  hidden behind Release `NDEBUG`):
  - `test_sockaddr_inspect::match_family_mismatch` -- test assumption
    didn't match actual behavior (brackets are syntactic, not semantic).
  - `test_call_count_limit::independent_functions` -- helper returned
    aliased static storage, making two contexts point at the same name.

## [2.1.0] - earlier

See git history for v2.1.0 release notes. Highlights: v2-everywhere
foundation (Linux x86_64+arm64, macOS Intel+arm64, Windows MSVC
x64+arm64, BSDs, Alpine); v1 source removed (ADR-0011); from-scratch
Windows trampoline (ADR-0009); AArch64 float params from day one
(ADR-0010).
