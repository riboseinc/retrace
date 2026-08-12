# Changelog

All notable changes to retrace are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
(see `docs/adr/0006-semantic-versioning.md`).

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
