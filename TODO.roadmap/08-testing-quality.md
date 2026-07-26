# 08 — Testing & quality

**Status**: [ ] pending
**Layer**: cross-cutting (touches every layer)
**Depends on**: 01
**Blocks**: nothing (parallel workstreams)

## Goal

Build a test pyramid that catches regressions at the right level:

1. **Unit tests** — every public function, every code path, fast feedback
2. **Integration tests** — each backend × each platform, real binaries
3. **Property tests** — fuzzing actions, config parsing, serialization round-trips
4. **Smoke tests** — CLI end-to-end on real targets
5. **Coverage** — gate at >85% on `src/core/` before merge

Plus continuous quality:
- AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer runs on CI
- clang-tidy / cppcheck on every PR
- Coverity Scan daily
- Memory leak check via valgrind (where available — not macOS)

## Why

The current testing is ad-hoc:

- `test/runtests.sh` is 95 lines of `retrace ./test/<name>` invocations — no
  assertions, exit codes, or output capture.
- `test/cmockatest.c` is the only structured test, gated on Linux + cmocka.
- No coverage measurement.
- No sanitizer runs.
- No CLI tests.

A library-grade product (TODO 05) needs library-grade tests.

## Architecture

```
test/
├── unit/                          # cmocka unit tests, fast
│   ├── core/
│   │   ├── test_engine.c
│   │   ├── test_script.c
│   │   ├── test_action_registry.c
│   │   ├── test_prototype_registry.c
│   │   └── test_thread_context.c
│   ├── config/
│   │   ├── test_json_source.c
│   │   ├── test_text_source.c
│   │   └── test_api_builder.c
│   ├── backends/
│   │   ├── test_preload_elf.c
│   │   └── test_preload_macho.c
│   └── cli/
│       └── test_args.c
├── integration/                   # real-process tests
│   ├── run_id_redirect.sh         # was: examples/id-redirection/test-*.sh
│   ├── run_getenv_fuzz.sh         # was: examples/getenv-fuzzing/test-*.sh
│   ├── run_http_redirect.sh       # was: test/httpredirect.sh
│   └── ...                        # (one per example)
├── property/                      # libfuzzer / AFL harnesses
│   ├── fuzz_json_config.c
│   ├── fuzz_text_config.c
│   └── fuzz_memory_action.c
├── golden/                        # golden-file round-trip tests
│   ├── conf_v1/                   # examples/*/retrace.conf* — must parse
│   └── conf_v2/                   # JSON equivalents — must round-trip
├── smoke/                         # CLI end-to-end
│   ├── smoke_run.sh
│   ├── smoke_validate.sh
│   └── smoke_backends.sh
└── CMakeLists.txt                 # registers all tests with CTest
```

Every test directory is registered with CTest via `add_test`. CTest labels
(`unit`, `integration`, `property`, `smoke`) allow running subsets:

```bash
ctest --test-dir build -L unit         # fast feedback loop
ctest --test-dir build -L integration  # full backend matrix
ctest --test-dir build -L smoke        # CLI end-to-end
ctest --test-dir build                 # all
```

## Test framework

- **cmocka** for unit tests (already a build dep, BSD-licensed, no ASSERT)
- **CTest** as the runner (CMake-native, no extra dep)
- **bash scripts** for integration / smoke (existing pattern, well understood)
- **libfuzzer** for property tests (clang-bundled, no external dep)

## Tasks

### [P0] Unit tests for core (TODO 02)
- [ ] `test/unit/core/test_engine.c` — create/destroy/set_script/get_script
- [ ] `test/unit/core/test_script.c` — add_intercept, validate, compose
- [ ] `test/unit/core/test_action_registry.c` — register, lookup, unknown
- [ ] `test/unit/core/test_prototype_registry.c` — lookup by name, glob match
- [ ] `test/unit/core/test_thread_context.c` — per-thread alloc, clear, reuse

### [P0] Unit tests for config (TODO 04)
- [ ] `test/unit/config/test_json_source.c` — parse valid/invalid buffers
- [ ] `test/unit/config/test_text_source.c` — parse every example v1 config
- [ ] `test/unit/config/test_api_builder.c` — programmatic construction

### [P0] Golden-file tests
- [ ] For each `examples/*/retrace.conf*`: parse with text source, serialize
  back to JSON, parse with JSON source, compare scripts for equality
- [ ] Catches regressions in either parser

### [P0] Convert existing integration tests
- [ ] Move `test/runtests.sh` body into individual `test/integration/run_*.sh`
- [ ] Each script exits 0 on success, non-zero on failure with diagnostic
- [ ] Assert on output content where possible (grep for expected log lines)

### [P0] Coverage
- [ ] CMake option `RETRACE_ENABLE_COVERAGE=ON` adds `--coverage` to CFLAGS
- [ ] `make coverage` target runs lcov, produces HTML report
- [ ] Codecov upload in CI (see TODO 09)
- [ ] Gate: `<85%` on `src/core/` fails CI

### [P0] Sanitizers
- [ ] CMake option `RETRACE_ENABLE_ASAN=ON`, `=UBSAN`, `=TSAN`, `=MSAN`
- [ ] CMake presets: `sanitize-asan`, `sanitize-tsan`, `sanitize-ubsan`
- [ ] CI runs sanitizer builds on every PR (see TODO 09)
- [ ] Known issue to handle: ASAN + LD_PRELOAD interact — document the expected
  invocation (run target under ASAN, run retrace library under ASAN, both
  must share ASAN runtime)

### [P1] Property tests
- [ ] `test/property/fuzz_json_config.c` — libfuzzer on JSON parser
- [ ] `test/property/fuzz_text_config.c` — libfuzzer on text parser
- [ ] `test/property/fuzz_memory_action.c` — verify malloc-fuzz at various rates

### [P1] Smoke tests
- [ ] `test/smoke/smoke_run.sh` — `retrace run -- /usr/bin/id` exits 0
- [ ] `test/smoke/smoke_validate.sh` — every example config validates
- [ ] `test/smoke/smoke_backends.sh` — `retrace backends list` lists expected set

### [P1] Static analysis
- [ ] `.clang-tidy` config (modernize, bugprone, performance, readability checks)
- [ ] CMake `CLANG_TIDY` target property on every library
- [ ] `cppcheck` target as alternative
- [ ] Coverity daily build (was: `.github/workflows/coverity.yml`)

### [P2] Performance regression tests
- [ ] Benchmark: `retrace run -- /usr/bin/true` overhead vs baseline `/usr/bin/true`
- [ ] Benchmark: 1M mallocs under `memory_fuzz` rate=0 vs no-retrace
- [ ] Catch >5% slowdown on hot paths

## Test running

```bash
# From repo root, after `cmake -B build -DRETRACE_BUILD_TESTS=ON`
cmake --build build
ctest --test-dir build                       # all
ctest --test-dir build -L unit -j8           # parallel unit
ctest --test-dir build -R test_json          # filter by name
ctest --test-dir build --output-on-failure   # show output
```

For sanitizers:
```bash
cmake --preset sanitize-asan -DRETRACE_BUILD_TESTS=ON
cmake --build --preset sanitize-asan
ctest --test-dir build-sanitize-asan
```

## Acceptance criteria

- Every public function in `src/core/`, `src/config/`, `src/backends/` has at
  least one unit test.
- `ctest --test-dir build` exits 0 on every supported platform.
- Coverage on `src/core/` ≥ 85%.
- ASAN / UBSAN / TSAN CI runs are green on a representative matrix.
- No new public function lands without a test (enforced by coverage gate).

## Open questions

- Do we adopt criterion instead of cmocka? cmocka is fine — no need to switch.
- Should we add Rust or Go integration tests for FFI bindings? Lean no for
  v2.0 — FFI bindings are a separate effort.
