# Developer's Guide

Companion to [`README.adoc`](README.adoc) (which covers user-facing
build, JSON config, and usage). This file is for contributors.


## Build (CMake only)

retrace uses CMake. Autotools and the `retrace` shell launcher were
removed at v2.1.0 (ADR-0011).

```sh
# standard developer build with tests + AddressSanitizer
cmake -B build -G Ninja \
    -DRETRACE_BUILD_TESTS=ON \
    -DRETRACE_ENABLE_ASAN=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Useful CMake options (see `CMakeLists.txt` for the canonical list):

| Option | Purpose |
|--------|---------|
| `RETRACE_BUILD_TESTS` | Build per-feature test binaries under `test/`. |
| `RETRACE_BUILD_EXAMPLES` | Build demos under `examples/`. |
| `RETRACE_ENABLE_ASAN` / `_UBSAN` / `_TSAN` | Sanitizer flags. |
| `RETRACE_ENABLE_COVERAGE` | Code coverage instrumentation. |
| `RETRACE_ENABLE_RPC` | Build the optional RPC subsystem. |

Feature probes (`CheckIncludeFile`, `CheckSymbolExists`,
`CheckCSourceCompiles`) populate `build/config.h` from
`cmake/config.h.cmake.in`. Per-OS selection happens via
`RETRACE_PLATFORM_{LINUX,DARWIN,FREEBSD,OPENBSD,NETBSD,WINDOWS,OHOS}`.


## Code style

* **`.clang-format`** (Mozilla-based). A pre-commit hook exists at
  `git-hooks/pre-commit.sh` but is currently disabled (`exit 0` at
  top) -- format manually if you want consistency.
* **`checkpatch.pl`** (Linux kernel, patched) runs on every PR via
  `.github/workflows/checkpatch.yml`. The ignore list lives in
  `ci/checkpatch.sh`; project-specific typedefs are in
  `typedefs.checkpatch`. `src/config/json/parson.{c,h}` is excluded
  (vendored third-party).
* **macOS minimum**: 10.12 (hardcoded in `CMakeLists.txt`).
* **OpenBSD**: `STAILQ_*` macros are aliased to `SIMPLEQ_*` via
  compile definitions. Go through the indirection; don't use `STAILQ_`
  directly.


## Test pyramid

CTest drives everything. Labels: `integration`, `unit`, `smoke`, `v2`.

```sh
ctest --test-dir build                 # run all
ctest --test-dir build -L integration  # only integration
ctest --test-dir build -L v2           # only v2-targeted
```

The integration runner is generated from `test/runtests.sh.in` via
`file(GENERATE)` at build time. It runs each per-feature binary under
`LD_PRELOAD`/`DYLD_INSERT_LIBRARIES` and reports pass/fail counts.

Skip logic for known failures lives in `should_skip()` inside
`test/runtests.sh.in`. Don't remove a skip without an issue tracker
reference.


## CI

GitHub Actions workflows under `.github/workflows/`:

| Workflow | Scope |
|----------|-------|
| `build.yml` | Linux x86_64 + aarch64, macOS Intel + arm64, Windows MSVC x86_64 + arm64 |
| `alpine.yml` | Alpine/musl x86_64 + aarch64 |
| `msys.yml` | MinGW64 + UCRT64 |
| `ohos.yml` | OHOS aarch64 (cross-compile + dockerharmony verify) |
| `nix.yml` | Nix flake build |
| `checkpatch.yml` | checkpatch.pl on the diff |
| `coverity.yml` | Daily scheduled scan |
| `release.yml` | Tag-triggered source + binary artifacts |

`.cirrus.yml` covers FreeBSD 13/14.

Local CI repro: `ci/main.sh`.


## Adding a new intercepted function

1. Add a `struct FuncPrototype` entry to the appropriate file under
   `src/core/prototypes/` (`stdio.c`, `stdlib.c`, `unistd.c`,
   `dirent.c`, `uio.c`, `signal.c`, `ctype.c`, `locale.c`). Specify
   name, calling convention, return type name, and per-parameter
   `ParamMeta` (name + format hint).
2. Add a `WRAPPER_ENTRY_SYSTEM_V` (or `_AArch64`) line in the
   matching `funcs_symbols.S` for every backend
   (`src/backends/preload_elf/{x86_64,aarch64}/`,
   `preload_macho/...`, `preload_bsd/...`).
3. The engine picks the prototype up automatically via the linker
   section scan in `retrace_funcs_init` (`src/core/funcs.c`).


## Adding a new action

1. Create `src/core/actions/<name>.c` (or extend `basic.c` /
   `memfuzz.c`).
2. Define the action as a `struct` with `.name`, `.callback`, and any
   parameter metadata.
3. Register via the action registry section macro -- the engine scans
   it at init. See `src/core/actions/basic.c` for the pattern.
4. The action is now referenceable from JSON config:
   `{ "action_name": "<name>", "action_params": {...} }`.

No engine change is needed -- that's the Open/Closed win.


## Adding a new backend

Each backend is one (OS, arch) combination. MECE: no overlap, no gap.

1. Create `src/backends/<name>/` with:
   - `backend.c` implementing `retrace_backend_t` (probe / spawn /
     attach / detach / translate_frame).
   - `{x86_64,aarch64}/arch_spec_top.S` -- the assembly trampoline.
   - `{x86_64,aarch64}/arch_spec_bottom.c` -- frame struct + real-impl
     resolution (`retrace_as_get_real_safe`).
   - `{x86_64,aarch64}/funcs_symbols.S` -- per-function table.
   - `CMakeLists.txt` registering an OBJECT library.
2. Self-register via the constructor-section scan in
   `src/backends/registry.c`.
3. `retrace_backend_select()` picks the highest-rank backend whose
   `probe()` succeeds. Set `.rank` appropriately.

See `docs/adr/0003-plugin-pattern-for-backends.md` and
`include/retrace/backend.h` for the contract.


## Real-impl indirection (reentrancy guard)

All internal libc usage inside retrace must go through
`retrace_real_impls.<fn>` (`src/core/real_impls.{c,h}`). These are
function pointers resolved once at init via `dlsym(RTLD_NEXT, ...)` /
the Darwin equivalent. **Bypass this and you recurse** -- the engine
will re-enter itself via the trampoline.

If you need a libc symbol that isn't yet in the table, add it to the
struct + init function, then use `retrace_real_impls.<new_fn>`.


## Init order

The library constructor (`src/core/main.c`) is order-sensitive:

```
retrace_as_init
  -> retrace_real_impls_init
  -> retrace_logger_init
  -> parson alloc hooks
  -> retrace_conf_init
  -> retrace_loger_update_config
  -> retrace_engine_init
  -> retrace_funcs_init
  -> retrace_datatypes_init
  -> retrace_actions_init
  -> retrace_as_init_late
```

Reorder at your peril -- later stages depend on earlier ones
(e.g. action callbacks need prototypes registered).


## Architecture Decision Records

Load-bearing decisions live under `docs/adr/`. Read these before
changing the corresponding subsystem:

| ADR | Topic |
|-----|-------|
| `0003` | Backend plugin pattern (`retrace_backend_t`) |
| `0006` | Semantic versioning |
| `0008` | Opaque public types for ABI stability |
| `0009` | From-scratch Windows inline hooking (no MinHook / Detours) |
| `0010` | AArch64 float params supported from day one |
| `0011` | v1 source removed at v2.1.0 (supersedes `0005`) |


## Security / bug hunting

### Static analysis

* **Coverity Scan** runs daily via `coverity.yml`. Results at
  https://scan.coverity.com/projects/riboseinc-retrace.
* **CodeQL** runs on every PR (see the workflow annotations).
* **Clang Static Analyzer**: `scan-build cmake --build build`.

### Dynamic analysis

* **ASan/UBSan**: `cmake -DRETRACE_ENABLE_ASAN=ON -DRETRACE_ENABLE_UBSAN=ON`.
* **TSan**: `cmake -DRETRACE_ENABLE_TSAN=ON` (POSIX only).
* **Coverage**: `cmake -DRETRACE_ENABLE_COVERAGE=ON` then
  `gcovr --xml-pretty --exclude-unreachable-branches --print-summary -o coverage.xml`.


## Code conventions

* Functions: `some_function` (snake_case).
* Variables: `some_variable`.
* Filenames: `malloc.c` (lowercase, no separators).
* Structs: `retrace_<thing>_t` (typedef'd, opaque where possible
  per ADR-0008).
* Constants (macro): `RTR_TEST_START`.
* Use `sizeof(variable)`, not `sizeof(type)`.
* Header guards: `#ifndef PATH_RELATIVE_H ...`.
* Declare functions `static` when not referenced outside the file.
* Use commit messages that close GitHub issues:
  `Fix XYZ. Closes #78.` (note: GitHub parses both `Closes #N.` and
  `Closes: #N` -- checkpatch warns on the colon variant.)
* **Never** add `Co-authored-by:` trailers for AI tools (per repo
  policy). Commits look like normal human work.
