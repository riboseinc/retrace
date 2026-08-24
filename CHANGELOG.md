# Changelog

All notable changes to retrace are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
(see `docs/adr/0006-semantic-versioning.md`).

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

## [2.32.0] — 2026-08-24

**`NtCreateFile` fixed: the `call_real` dispatch covered 0..6
arguments** (TODO.trace-profile/28) — the static-binary smoke
restores its full assertion.

- Root cause (source-confirmed after the bisect isolated
  `NtCreateFile`-alone): `retrace_as_call_real_dispatch`'s
  switch ended at 6 args ("no known libc symbol needs more" —
  true until the ntdll layer); 11-param `NtCreateFile` hit
  `default: ret_val = -1` — the real function was never called
  and the synthesized −1 surfaced as a clean failure. Exactly
  the bisect signature: any script, no crash, `NtOpenFile` (6
  params) fine.
- Fix: dispatch extended to 0..12 (NtQueryDirectoryFile's
  ceiling); per-arity unit tests (a 12-arg callee through
  narrower signatures reads caller-stack garbage — the test
  family enforces per-arity round-trips). 90/90 tests.
- The CI smoke now ASSERTS the full original claim: a
  static-CRT (/MT) binary's file activity, captured with
  decoded paths through the ntdll layer.

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
