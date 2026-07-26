# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What retrace is

`retrace` is a userspace security/vulnerability discovery tool that intercepts libc calls in dynamically-linked ELF (Linux, FreeBSD, OpenBSD, NetBSD) and Mach-O (macOS) binaries. It works by preloading a shared library into the target process (`LD_PRELOAD` on ELF, `DYLD_INSERT_LIBRARIES` on Darwin) and either logging or rewriting each intercepted call's arguments and return value. Use cases: reverse engineering, debugging, fuzzing (malloc failure injection, getenv buffer-overflow/format-string/garbage fuzzing, incomplete I/O), redirecting network connects, redirecting file opens, faking OpenSSL verify results, etc.

The repo ships **two parallel implementations**, both built from the same Autotools tree:

- **v1** (default): per-function C wrappers. Each intercepted libc symbol is reimplemented in `src/v1/<area>.c`, calls `retrace_log_and_redirect_before` → `real_<fn>` → `retrace_log_and_redirect_after`, and is published as the symbol via the `RETRACE_REPLACE` macro. Configuration is a line-oriented text file (`retrace.conf`).
- **v2** (opt-in via `--enable-v2`): a single assembly trampoline per function that funnels every call into `retrace_engine_wrapper`, which then dispatches to a JSON-driven script of named actions (`log_params`, `call_real`, `modify_in_param_str/int/arr`, `modify_return_value_int`, `memory_fuzz`). Per-function prototypes live in `src/v2/prototypes/`; per-arch trampolines live in `src/v2/arch/x86-64/{linux,osx,bsd}/`. Configuration is a JSON file pointed to by `RETRACE_JSON_CONFIG`.

v2 is the active development direction. v1 is the supported, documented release. macOS SIP blocks `DYLD_INSERT_LIBRARIES` for binaries in system directories — `csrutil disable` is required to trace them.

## Modernization context (2026-07)

The user is planning to **modernize the repo into a clean library + CLI product**, with updated GHA workflows, modern packaging (vcpkg, CMake), first-class library consumption by third parties, and a CLI wrapper. Treat this as the strategic frame when touching build system, packaging, or public API: changes that tighten the library/CLI boundary or replace ad-hoc Autotools flow with CMake/conan/vcpkg are aligned; changes that further entrench v1-only Autotools quirks are not. All actual modernization work must go through PRs on feature branches — never commit to `main`.

## Build

The build is Autotools. `autogen.sh` runs `autoreconf -ivf` (and the `configure.ac` calls `AM_CONDITIONAL` for `[LINUX]`, `[DARWIN]`, `[FREEBSD]`, `[OPENBSD]`, so per-platform code selection happens at configure time).

```sh
sh autogen.sh
./configure --enable-tests            # v1, with tests
./configure --enable-v2               # v2 library only
./configure --enable-v2 --enable-v2_wrapper   # also build the retrace2 CLI (needs libnereon)
make
make check                            # runs test/runtests.sh (+ cmockatest on Linux)
sudo make install
```

Useful configure flags:
- `--enable-tests` — builds the per-feature test binaries in `test/` (off by default; users don't need them)
- `--enable-v2` — switch the active implementation from v1 to v2 (`src/Makefile.am` `SUBDIRS` switches between `v1/` and `v2/`)
- `--enable-v2_wrapper` — build the `retrace2` binary; requires libnereon (builds via `ci/install_functions.sh::install_libnereon`)
- `--with-cmocka=PATH` — location of cmocka install (only used by Linux cmocka tests)
- `--with-openssl=PATH` — non-default OpenSSL root
- `--enable-debug` — adds `-g3 -DDEBUG` (note: `configure.ac` forces `-O0` unconditionally because `-O2` was crashing v2)
- `--enable-rpc` — builds `rpc/` subtree (an RPC/IPC layer; off by default)

After editing any `configure.ac` or `Makefile.am`, **re-run `autogen.sh`**.

`configure.ac` has many `AC_CHECK_DECLS`/`AC_CHECK_TYPES` probes for `fcntl`, `socket`, and `ptrace` features — these populate `config.h` and gate per-platform parameter-type macros in `src/v1/common.h`.

## Running

```sh
retrace [--config <path>] [--lib <path>] <binary> [args...]   # v1 launcher (forks + LD_PRELOAD)
RETRACE_JSON_CONFIG=<conf.json> LD_PRELOAD=src/v2/.libs/libretrace_v2.so <binary>   # v2 raw
./retrace2 -c <config.json> -p '<binary> [args]'             # v2 wrapper (needs libnereon)
```

Config file path can be set via `-f` (or `RETRACE_CONFIG` env for v1). `RETRACE_JSON_CONFIG` for v2. v2 also reads `RETRACE_LOGGER_DEF_ENA`, `RETRACE_LOGGER_DEF_STDOUT_ENA`, `RETRACE_LOGGER_DEF_FN` to control log output. `RETRACE_CLI=1` (Linux, v1) enables a PTS-attached interactive CLI — connect with `minicom -p /dev/pts/N`.

## Tests

`make check` invokes `test/runtests.sh`, which:
1. Builds one test binary per libc area (`test/id`, `test/env`, `test/file`, ... — see `test/Makefile.am`).
2. Runs each under v1 via `retrace ./<binary>`.
3. If `src/v2/.libs/libretrace_v2.{so,dylib}` exists, re-runs each under v2 via `LD_PRELOAD`/`DYLD_INSERT_LIBRARIES`.

Run a single test:
```sh
retrace ./test/id                # v1
LD_PRELOAD=src/v2/.libs/libretrace_v2.so ./test/id    # v2
```

Linux cmocka tests live in `test/cmockatest.c` and are only built/used when cmocka is detected (`USE_CMOCKA`).

Examples under `examples/*/` (dns-fuzz, getenv-fuzzing, http-server-overflow, id-redirection, net-fuzzing, stringinject, unsafe-system) are self-contained demos with their own `test-*.sh` scripts and `*.conf` retrace configs.

## CI

Multi-platform matrix driven by `ci/main.sh` (dispatches `v1`/`v2`/`v2wrapper` build variants) and `ci/before_install.sh` (per-OS package setup in `ci/before_install/{linux,darwin,msys,freebsd,openbsd,netbsd}.sh`). `ci/install_functions.sh` builds external deps (cmocka, libnereon, checkpatch.pl). Workflows:

- `.github/workflows/ubuntu.yml`, `macos.yml`, `windows.yml` (msys2) — run `ci/main.sh` against `v1`, `v2`, `v2wrapper` on each OS.
- `.github/workflows/nix.yml` — builds `packages.v1/v2/v2wrapper` from `flake.nix`.
- `.github/workflows/checkpatch.yml` — runs Linux kernel `checkpatch.pl` (patched) against the diff; ignore-list and typedefs in `ci/checkpatch.sh`.
- `.github/workflows/coverity.yml` — daily scheduled scan; uploads to scan.coverity.com.
- `.cirrus.yml` — FreeBSD 12.3 / 13.

The matrix runner is `./ci/main.sh {v1,v2,v2wrapper}`. Reproduce locally with the same script.

## v1 architecture deep-dive

- **Interposition**: `src/v1/common.h` defines `RETRACE_REPLACE(func, type, defn, args)`. On Linux this defines `rtr_fixup_##func` that lazily resolves `real_<func>` via `_dl_sym(RTLD_NEXT, ...)` (with an atomic-builtins fast path), then exports the wrapper as `func`. On macOS it uses `DYLD_INTERPOSE` (a `__DATA,__interpose` section entry). BSDs resolve via `dlsym(RTLD_NEXT, ...)`.
- **Per-function pattern** (e.g. `src/v1/id.c`): declare a local `struct rtr_event_info` describing parameter types/values and return type, call `retrace_log_and_redirect_before(&event_info)`, call `real_<func>(...)`, set `logging_level |= RTR_LOG_LEVEL_ERR` on failure, call `retrace_log_and_redirect_after(&event_info)`. The `RETRACE_REPLACE` macro at the bottom of each function wires it into the symbol table.
- **Reentrancy guard**: `common.c` keeps `g_enable_tracing` (per-thread via `pthread_key_t` on non-BSD, `__thread` on FreeBSD, fixed array on OpenBSD) to prevent the tracer's own libc calls from recursively entering the wrappers. Use `trace_disable()` / `trace_restore(old)` around any internal libc call.
- **Config**: text lines parsed by `rtr_get_config_single`/`rtr_get_config_multiple` (variadic, terminated by `ARGUMENT_TYPE_END`). Each function declares the types it expects. See `src/v1/retrace.conf.example` for the full vocabulary (`getuid,0`, `connect,src,dst`, `fopen,from,to`, `SSL_get_verify_result,10`, `memoryfuzzing,0.05`, `incompleteio,10`, `showtimestamp`, `logging-global,...`, `fuzzing-getenv,...`, etc.).
- **Function groups & levels** in `common.h`: `RTR_FUNC_GRP_{MEM,FILE,NET,SYS,STR,SSL,PROC,TEMP,ALL}` and `RTR_LOG_LEVEL_{NOR,ERR,FUZZ,REDIRECT,ALL}`. `logging-global,<groups>,<levels>` filters which calls get printed.
- **`retrace_main.c`** is a `__attribute__((constructor))` that, when `RETRACE_CLI=1`, opens a PTS, registers commands via `cli_register_command_blk`, and spawns a cli thread.

## v2 architecture deep-dive

- **Per-arch assembly trampoline** (`src/v2/arch/x86-64/{linux,osx,bsd}/arch_spec_top.S`): `WRAPPER_ENTRY_SYSTEM_V <func>` emits a symbol `<func>` that pushes the SysV-ABI register arguments into a `WrapperSystemVFrame`, calls `retrace_engine_wrapper(func_name, frame)`, then either tail-calls the real implementation (if the engine set `call_real_flag`) or returns the synthesized `ret_val`.
- **Function inventory**: `src/v2/funcs_symbols.S` is a flat list of `WRAPPER_ENTRY_SYSTEM_V` lines covering every intercepted libc symbol — this is the canonical list of what v2 supports. Add a new function by adding a line here **and** a prototype entry below.
- **Prototypes**: `src/v2/prototypes/<header>.c` are arrays of `struct FuncPrototype` (`name`, `conv`, `type_name`, `params[]` of `struct ParamMeta`). They're installed via `retrace_func_define_prototypes(<header>)` into a linker section the engine scans at init.
- **Engine** (`src/v2/engine.c`): per-thread `struct ThreadContext` holds the prototype, real impl ptr, and parsed params. `retrace_engine_wrapper` looks up the matching `intercept_script` in the JSON config and runs its `actions` in order.
- **Actions** (`src/v2/actions/basic.c`, `memfuzz.c`): registered by name into a `__DATA,__retrace_acts` section. Built-ins: `log_params`, `call_real`, `modify_in_param_str`, `modify_in_param_int`, `modify_in_param_arr`, `modify_return_value_int`, `memory_fuzz`. New behaviors = new action, no engine change.
- **Real-impl indirection** (`real_impls.{c,h}`): all internal libc usage inside v2 goes through `retrace_real_impls.<fn>` function pointers, resolved once at init via `dlsym(RTLD_NEXT, ...)`. This is the v2 equivalent of v1's reentrancy guard — bypass it and you recurse.
- **Init order matters** (`src/v2/main.c` constructor): `retrace_as_init` → `retrace_real_impls_init` → `retrace_logger_init` → parson alloc hooks → `retrace_conf_init` → `retrace_loger_update_config` → `retrace_engine_init` → `retrace_funcs_init` → `retrace_datatypes_init` → `retrace_actions_init` → `retrace_as_init_late`.
- **Config** (`src/v2/conf.c`): if `RETRACE_JSON_CONFIG` is set, parse that file; otherwise use the hardcoded default (`log_params` + `call_real` for `func_name: "*"`). Uses `parson` (vendored in `src/v2/parson.{c,h}`) with comment-tolerant parsing.
- **`retrace_v2.c`** is the `retrace2` CLI launcher — only built when `--enable-v2_wrapper` (depends on libnereon for arg parsing via `retrace_v2.nos`).

## Conventions

- **`common.h` is always the first include** in v1 .c files — it pulls in `config.h`, sets `_GNU_SOURCE`, and defines the `RETRACE_*` macros. Don't reorder.
- **Code style**: `.clang-format` (Mozilla-based). `git-hooks/pre-commit.sh` exists but is currently disabled (`exit 0` at top) — format manually if you want consistency.
- **checkpatch**: GitHub checks via Linux `checkpatch.pl` with a long ignore list in `ci/checkpatch.sh`. New typedefs go in `typedefs.checkpatch` (currently `JSON_Object`). `src/v2/parson.{c,h}` is excluded (vendored third-party).
- **macOS minimum**: 10.12 (hardcoded in `configure.ac`).
- **Don't edit generated files**: `configure`, `Makefile.in`, `config.h.in`, `aclocal.m4`, `ltmain.sh`, etc. — they're produced by `autogen.sh` and committed for convenience but regenerated on any change.
- **Don't edit `retrace`** (the launcher script) directly if it were ever templated; today `src/v1/retrace.c` is the source of truth and the launcher is the compiled binary.
- **OpenBSD quirks**: `configure.ac` `#define`s `STAILQ_*` to `SIMPLEQ_*` for OpenBSD. Don't use `STAILQ_` macros directly without going through the indirection.

## Key files to know

| Path | Purpose |
|------|---------|
| `configure.ac` | All build switches, per-OS conditionals, feature probes |
| `src/Makefile.am` | Switches `SUBDIRS` between `v1` and `v2` based on `--enable-v2` |
| `src/v1/common.h` | `RETRACE_REPLACE`/`RETRACE_IMPLEMENTATION` macros, event struct, all parameter-type constants, group/level enums |
| `src/v1/common.c` | Reentrancy guard, config parser, descriptor tracking, logging |
| `src/v1/retrace.c` | v1 launcher binary (`main` → fork + `LD_PRELOAD`) |
| `src/v1/retrace_main.c` | v1 library constructor (CLI init if `RETRACE_CLI=1`) |
| `src/v1/retrace.conf.example` | v1 config-file vocabulary reference |
| `src/v1/util/retrace-gen.py` | Helper script for generating wrapper boilerplate from C prototypes |
| `src/v2/funcs_symbols.S` | Canonical list of every function v2 intercepts |
| `src/v2/prototypes/<h>.c` | Per-header prototype tables consumed by the engine |
| `src/v2/arch/x86-64/<os>/arch_spec_top.S` | Per-arch assembly trampoline (the heart of v2) |
| `src/v2/engine.{c,h}` | `retrace_engine_wrapper` — the central dispatch |
| `src/v2/actions/{basic,memfuzz}.c` | Built-in actions; extend here for new behaviors |
| `src/v2/real_impls.{c,h}` | All libc usage inside v2 must go through `retrace_real_impls.*` |
| `src/v2/main.c` | v2 library constructor — strict init order |
| `ci/main.sh` | Drives the CI matrix; reproduce locally with this |
| `flake.nix` / `nix/*.nix` | Nix packaging (v1, v2, v2wrapper as separate packages) |

## Tools

- `tools/spawn/` — concurrent-process spawner for stress-testing
- `tools/stringinjector/` — file/string injection helper (used by `test/strinject.sh`)
- `rpc/` — opt-in RPC layer (built with `--enable-rpc`); has templated function tables (`functions.{c,h}.template`, `handlers.c.template`, `shim.{c,h}.template`)
