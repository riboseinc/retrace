# Changelog

All notable changes to retrace are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
(see `docs/adr/0006-semantic-versioning.md`).

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
