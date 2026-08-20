# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What retrace is

`retrace` is a userspace security/vulnerability discovery tool that intercepts libc calls in dynamically-linked ELF (Linux, FreeBSD, OpenBSD, NetBSD) and Mach-O (macOS) binaries. It works by preloading a shared library into the target process (`LD_PRELOAD` on ELF, `DYLD_INSERT_LIBRARIES` on Darwin) and either logging or rewriting each intercepted call's arguments and return value. Use cases: reverse engineering, debugging, fuzzing (malloc failure injection, getenv buffer-overflow/format-string/garbage fuzzing, incomplete I/O), redirecting network connects, redirecting file opens, faking OpenSSL verify results, etc.

The single implementation is **v2**: a single assembly trampoline per function that funnels every call into `retrace_engine_wrapper`, which dispatches to a JSON-driven script of named actions (`log_params`, `call_real`, `modify_in_param_str/int/arr`, `modify_return_value_int`, `memory_fuzz`). Per-function prototypes live in `src/core/prototypes/`; per-arch trampolines live in `src/backends/preload_*/{x86_64,aarch64}/`. Configuration is a JSON file pointed to by `RETRACE_JSON_CONFIG`.

(v1 was removed in Phase 9 per ADR-0011; Autotools tree was deleted in the same phase. CMake is the only build system.)

macOS SIP blocks `DYLD_INSERT_LIBRARIES` for binaries in system directories — `csrutil disable` is required to trace them.

## Build

CMake is the canonical build. vcpkg manifest mode pulls OpenSSL + cmocka automatically on Windows; system packages provide them on Linux/macOS/BSDs.

```sh
cmake -B build -G Ninja -DRETRACE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

Useful CMake options:

- `RETRACE_BUILD_V2` (default ON) — build the retrace shared library (`libretrace.{so,dylib}`). The internal CMake target name is `retrace_v2` for historical reasons (v1 source was deleted in Phase 9 / ADR-0011; only one library remains).
- `RETRACE_BUILD_CLI` (default ON) — build the CLI launcher (placeholder; new CLI lands in Phase 10).
- `RETRACE_BUILD_TESTS` (default OFF) — build the per-feature test binaries in `test/`.
- `RETRACE_BUILD_EXAMPLES` (default OFF) — build the demos under `examples/`.
- `RETRACE_ENABLE_RPC` (default OFF) — build the `rpc/` subtree.
- `RETRACE_ENABLE_ASAN` / `RETRACE_ENABLE_UBSAN` / `RETRACE_ENABLE_TSAN` / `RETRACE_ENABLE_COVERAGE` — instrumentation toggles (see `cmake/Sanitizers.cmake`).

The CMake build does feature probes (`CheckIncludeFile`, `CheckSymbolExists`, `CheckCSourceCompiles`) that populate `config.h` from `cmake/config.h.cmake.in`. Per-OS selection happens via `RETRACE_PLATFORM_{LINUX,DARWIN,FREEBSD,OPENBSD,NETBSD,WINDOWS}`.

## Running

```sh
RETRACE_JSON_CONFIG=<conf.json> LD_PRELOAD=build/src/v2/libretrace.so <binary>
```

v2 reads `RETRACE_JSON_CONFIG` (path to JSON config; defaults to log_params+call_real for all funcs), `RETRACE_LOGGER_DEF_ENA`, `RETRACE_LOGGER_DEF_STDOUT_ENA`, `RETRACE_LOGGER_DEF_FN` (log output control).

## Tests

CTest drives the test pyramid. Labels: `integration`, `unit`, `smoke`, `v2`.

```sh
ctest --test-dir build                 # run all
ctest --test-dir build -L integration  # only integration
ctest --test-dir build -L v2           # only v2-targeted
```

The integration runner is generated from `test/runtests.sh.in` via `file(GENERATE)` (build-time generator expression substitution). It runs each per-feature binary under v2 via `LD_PRELOAD`/`DYLD_INSERT_LIBRARIES` and reports pass/fail counts.

Examples under `examples/*/` (dns-fuzz, getenv-fuzzing, http-server-overflow, id-redirection, net-fuzzing, stringinject, unsafe-system) are self-contained demos.

## CI

GHA workflows under `.github/workflows/`:

- `build.yml` — Linux x64+arm64, macOS Intel+arm64, Windows MSVC x64+arm64 (matrix); CMake + ctest.
- `alpine.yml` — Alpine/musl x64+arm64 (container + qemu).
- `msys.yml` — MinGW64 + UCRT64.
- `nix.yml` — builds `packages.v2/v2wrapper` from `flake.nix`.
- `checkpatch.yml` — Linux kernel `checkpatch.pl` (patched) against the diff; ignore-list in `ci/checkpatch.sh`.
- `coverity.yml` — daily scheduled scan; uploads to scan.coverity.com.
- `release.yml` — tag-triggered source tarball + binary artifacts.

`.cirrus.yml` covers FreeBSD 13/14.

## v2 architecture deep-dive

- **Per-arch assembly trampoline** (`src/backends/preload_*/{x86_64,aarch64}/arch_spec_top.S`): `WRAPPER_ENTRY_SYSTEM_V` (or `_AArch64`) emits a symbol `<func>` that pushes the SysV-ABI register arguments into a `WrapperSystemVFrame`, calls `retrace_engine_wrapper(func_name, frame)`, then either tail-calls the real implementation (if the engine set `call_real_flag`) or returns the synthesized `ret_val`.
- **Function inventory**: per-backend `funcs_symbols.S` files are flat lists of `WRAPPER_ENTRY_*` lines covering every intercepted libc symbol — this is the canonical list of what v2 supports. Add a new function by adding a line here **and** a prototype entry below.
- **Prototypes**: `src/core/prototypes/<header>.c` are arrays of `struct FuncPrototype` (`name`, `conv`, `type_name`, `params[]` of `struct ParamMeta`). They're installed via `retrace_func_define_prototypes(<header>)` into a linker section the engine scans at init.
- **Engine** (`src/core/engine.c`): per-thread `struct ThreadContext` holds the prototype, real impl ptr, and parsed params. `retrace_engine_wrapper` looks up the matching `intercept_script` in the JSON config and runs its `actions` in order.
- **Actions** (`src/core/actions/basic.c`, `memfuzz.c`): registered by name into a `__DATA,__retrace_acts` section (or ELF equivalent). Built-ins: `log_params`, `call_real`, `modify_in_param_str`, `modify_in_param_int`, `modify_in_param_arr`, `modify_return_value_int`, `memory_fuzz`. New behaviors = new action, no engine change.
- **Real-impl indirection** (`src/core/real_impls.{c,h}`): all internal libc usage inside v2 goes through `retrace_real_impls.<fn>` function pointers, resolved once at init via `dlsym(RTLD_NEXT, ...)`. This is the reentrancy guard — bypass it and you recurse.
- **Init order matters** (`src/core/main.c` constructor): `retrace_as_init` → `retrace_real_impls_init` → `retrace_logger_init` → parson alloc hooks → `retrace_conf_init` → `retrace_loger_update_config` → `retrace_engine_init` → `retrace_funcs_init` → `retrace_datatypes_init` → `retrace_actions_init` → `retrace_as_init_late`.
- **Config** (`src/config/json/conf.c`): if `RETRACE_JSON_CONFIG` is set, parse that file; otherwise use the hardcoded default (`log_params` + `call_real` for `func_name: "*"`). Uses `parson` (vendored in `src/config/json/parson.{c,h}`) with comment-tolerant parsing.
- **Backend plugin system** (`src/backends/`): each (OS, arch) combo has its own backend (`preload_elf`, `preload_macho`, `preload_bsd`, `preload_msvc`, `preload_mingw`, `ptrace`). Backends self-register via constructor-section scan; `retrace_backend_select()` picks the highest-rank backend whose `probe()` succeeds. On Windows the preload-msvc/mingw target IS the library (one injectable `retrace.dll`: engine + `win_common` hook core + backend; no `retrace_v2` target). Registry arrays land in short PE sections (`.rtrA`/`.rtrF`/`.rtrD`) walked via the module's own PE headers (`win_common/section_walk.c`) — PE has no `__start_` symbols and MSVC has no constructors. `DllMain` installs hooks BEFORE `retrace_core_boot()` so hooked names resolve to their trampolines. ntdll hooks are opt-in via `RETRACE_WIN_NTDLL=1` (AV/EDR sensitivity).

## Conventions

- **Code style**: `.clang-format` (Mozilla-based). `git-hooks/pre-commit.sh` exists but is currently disabled (`exit 0` at top) — format manually if you want consistency.
- **checkpatch**: GitHub checks via Linux `checkpatch.pl` with a long ignore list in `ci/checkpatch.sh`. `typedefs.checkpatch` carries project-specific typedefs. `src/config/json/parson.{c,h}` is excluded (vendored third-party).
- **macOS minimum**: 10.12 (hardcoded in `CMakeLists.txt`).
- **OpenBSD quirks**: `CMakeLists.txt` `add_compile_definitions` aliases `STAILQ_*` to `SIMPLEQ_*` for OpenBSD. Don't use `STAILQ_` macros directly without going through the indirection.

## Key files to know

| Path | Purpose |
|------|---------|
| `CMakeLists.txt` | All build switches, per-OS conditionals, feature probes |
| `cmake/config.h.cmake.in` | config.h template (populated by CMake feature probes) |
| `cmake/Sanitizers.cmake` | ASAN/UBSAN/TSAN/coverage flag helpers |
| `include/retrace/backend.h` | Public backend plugin API (`retrace_backend_t`) |
| `include/retrace/retrace.h` | Public library API (opaque types per ADR-0008) |
| `src/core/engine.{c,h}` | `retrace_engine_wrapper` — the central dispatch |
| `src/core/main.c` | v2 library constructor — strict init order |
| `src/core/real_impls.{c,h}` | All libc usage inside v2 must go through `retrace_real_impls.*` |
| `src/core/prototypes/<h>.c` | Per-header prototype tables consumed by the engine |
| `src/core/actions/{basic,memfuzz}.c` | Built-in actions; extend here for new behaviors |
| `src/backends/registry.c` | Backend registry + `retrace_backend_select` |
| `src/backends/preload_*/{x86_64,aarch64}/arch_spec_top.S` | Per-arch assembly trampoline |
| `src/backends/preload_*/<arch>/funcs_symbols.S` | Per-backend function inventory |
| `src/config/json/conf.c` | JSON config parser (vendored parson) |
| `ci/main.sh` | Local CI repro |
| `ci/checkpatch.sh` | checkpatch.pl driver + ignore list |
| `flake.nix` / `nix/*.nix` | Nix packaging (v2, v2wrapper) |

## ADRs

Architecture decisions are in `docs/adr/`. Notable:

- `0005-v2-as-future-v1-deprecated.md` (superseded by 0011)
- `0006-semantic-versioning.md`
- `0008-opaque-public-types.md`
- `0009-from-scratch-windows-trampoline.md`
- `0010-aarch64-float-params-from-day-one.md`
- `0011-v1-removal-at-v2.1.0.md` — supersedes 0005; v1 source removed at v2.1.0

## Tools

- `tools/spawn/` — concurrent-process spawner for stress-testing
- `tools/stringinjector/` — file/string injection helper
- `rpc/` — opt-in RPC layer (built with `RETRACE_ENABLE_RPC=ON`); has templated function tables (`functions.{c,h}.template`, `handlers.c.template`, `shim.{c,h}.template`)
