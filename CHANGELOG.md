# Changelog

All notable changes to retrace are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
(see `docs/adr/0006-semantic-versioning.md`).

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

- Unit tests for all 14 built-in actions: `addr_deny`,
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
