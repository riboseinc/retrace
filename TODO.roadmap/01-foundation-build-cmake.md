# 01 — Foundation: CMake build, repo hygiene

**Status**: [~] in progress
**Layer**: 1 (foundation — everything else builds on this)
**Depends on**: nothing
**Blocks**: 02, 03, 04, 05, 06, 07, 08, 09

## Goal

Replace Autotools with CMake as the primary build system, while keeping
Autotools functional until Phase 3. Make `find_package(retrace)` work for
downstream consumers and `cmake --build` work on every supported platform.

## Why

- Autotools is increasingly poorly understood by new contributors; CMake is
  the lingua franca of modern C/C++ packaging.
- vcpkg, conan, and system package managers all expect CMake (or a CMake
  wrapper) to be the canonical build.
- IDE integration (VS Code, CLion, Visual Studio) is far better with CMake.
- Per-platform cross-compilation (e.g., Linux ARM64, Windows ARM64) is more
  uniform with CMake toolchain files than with `configure.ac` `case` blocks.
- The existing `configure.ac` has 130+ lines of `AC_CHECK_DECLS` probes for
  features that are universally present on supported platforms (2017-era
  portability paranoia). CMake's `check_symbol_exists` is more concise.

## Architecture

```
retrace/
├── CMakeLists.txt                 # Top-level: project(), options, add_subdirectory
├── CMakePresets.json              # Debug/Release/Sanitize/Analyze presets
├── vcpkg.json                     # Manifest-mode vcpkg deps (OpenSSL, cmocka)
├── cmake/
│   ├── retrace-config.cmake.in    # Template for find_package(retrace)
│   ├── retraceTargets.cmake       # Exported targets
│   ├── retrace-config-version.cmake.in
│   ├── FindCmocka.cmake           # Local find module until upstream vcpkg port is OK
│   ├── Platform/Linux.cmake       # was: configure.ac islinux=yes
│   ├── Platform/Darwin.cmake      # was: configure.ac isdarwin=yes + -mmacosx-version-min
│   ├── Platform/FreeBSD.cmake
│   ├── Platform/OpenBSD.cmake     # was: STAILQ_* → SIMPLEQ_* defines
│   ├── Platform/MSYS.cmake
│   └── Sanitizers.cmake           # -fsanitize=address,undefined,thread
├── include/retrace/               # Public headers (TODO 05)
│   ├── retrace.h
│   └── version.h
├── src/
│   ├── core/                      # Layer 2 (TODO 02) — engine, script, action
│   ├── backends/                  # Layer 3 (TODO 03)
│   │   ├── preload_elf/           # was: src/v2/arch/x86-64/linux/
│   │   ├── preload_macho/         # was: src/v2/arch/x86-64/osx/
│   │   └── preload_bsd/           # was: src/v2/arch/x86-64/bsd/
│   ├── config/                    # Layer 4 (TODO 04)
│   │   ├── json/                  # was: src/v2/conf.c + parson
│   │   └── text/                  # was: src/v1/config parsing
│   ├── cli/                       # Layer 5/6 (TODO 06) — was: src/v1/retrace.c, src/v2/retrace_v2.c
│   ├── v1/                        # legacy (TODO 10) — unchanged during transition
│   └── v2/                        # current code being refactored into above layers
├── test/                          # TODO 08 — restructure into unit/integration
└── packaging/                     # TODO 07 — vcpkg port, conan recipe, rpm spec
```

## Tasks

### [P0] CMake skeleton, parallel to Autotools
- [x] Create `CMakeLists.txt` at root (project, options, finds)
- [x] Create `CMakePresets.json` with `debug`, `release`, `sanitize` presets
- [x] Create `vcpkg.json` manifest with OpenSSL + cmocka deps
- [x] Create `cmake/Sanitizers.cmake`
- [ ] Define exported targets `retrace::core`, `retrace::cli`, `retrace::v1-compat`
- [ ] Write `cmake/retrace-config.cmake.in` for `find_package(retrace)`

### [P0] v1 builds under CMake (parity with current `make`)
- [ ] `src/v1/CMakeLists.txt` builds `libretrace.so` / `libretrace.dylib`
- [ ] Top-level builds `retrace` binary (was: `bin_PROGRAMS=retrace`)
- [ ] Feature probes mirror `configure.ac` (`check_symbol_exists` for `O_TMPFILE`, `PTRACE_*`, `SO_*`, etc.)
- [ ] macOS minimum 10.12 (was: `configure.ac` `-mmacosx-version-min=10.12`)
- [ ] OpenBSD `STAILQ_*` → `SIMPLEQ_*` rewrite in a Platform include
- [ ] `make check` equivalent via CTest

### [P0] v2 builds under CMake
- [ ] `src/v2/CMakeLists.txt` builds `libretrace_v2.so` with per-arch ASM sources
- [ ] Conditional sources per `CMAKE_SYSTEM_NAME` (Linux/Darwin/FreeBSD)
- [ ] Optional `retrace2` CLI gated on `RETRACE_BUILD_CLI` (replaces libnereon dependency eventually)

### [P1] Repo hygiene
- [x] Move generated Autotools artifacts out of source tree (`.libs/`, `*.lo`, `*.o`) — already gitignored
- [ ] Remove tracked `autom4te.cache/` from repo (it's regenerated, shouldn't be in VCS) — **verify no consumer depends on it before removing**
- [ ] Remove tracked `Makefile.in`, `aclocal.m4`, `config.h.in`, `configure`, `ltmain.sh`, etc. — these are regenerable; document `sh autogen.sh` in CONTRIBUTING
- [ ] Verify the untracked `a.txt`, `-h`, `ci/env-openbsd.sh.orig` are intentional or move to a scratch directory — **ask user, do not delete** (global rule)

### [P1] Modern tooling
- [ ] Replace `.clang-format` (Mozilla 2017) with a 2025 baseline (LLVM 18+)
- [ ] Re-enable `git-hooks/pre-commit.sh` (currently `exit 0`) with docker-free `clang-format` direct invocation
- [ ] Add `clang-tidy` config (`YAML` file) and integrate as CMake target
- [ ] Add `.editorconfig`

### [P2] Build performance
- [ ] Ninja as default generator (`-G Ninja`)
- [ ] ccache integration (`CMAKE_C_COMPILER_LAUNCHER=ccache`)
- [ ] Unity builds as optional fast-path (`CMAKE_UNITY_BUILD=ON`)

## Decisions to capture in ADRs (TODO 12)

- ADR-0001: CMake replaces Autotools as primary build (this file)
- ADR-0002: vcpkg manifest mode for dependencies (justification: cross-platform, no system pkg pollution)
- ADR-0003: Both build systems coexist during Phase 1-2 (de-risk migration)

## Open questions

- Should the CMake build also produce Autotools-compatible `*.la` files for
  distros that consume them? Probably no — modern distros use CMake directly.
- Do we keep `libtool` at all in the new world? It exists for `.la` convenience
  libraries; CMake handles this natively. Plan: drop `libtool` in Phase 3.

## Acceptance criteria

- `cmake -B build -DRETRACE_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build`
  produces the same artifacts as `./configure --enable-tests && make && make check`.
- `find_package(retrace CONFIG REQUIRED)` works in an external project and
  links `retrace::core`.
- Build works on: Ubuntu 22.04/24.04, macOS 13/14 (Intel + ARM), Windows 2022
  (MSVC + MinGW via vcpkg), Alpine 3.21, FreeBSD 13/14.
- No new files committed to source tree without a corresponding test (TODO 08).
