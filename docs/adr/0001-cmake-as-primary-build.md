# ADR-0001: CMake as primary build system

- **Status**: accepted
- **Date**: 2026-07-26
- **Supersedes**: Autotools build (`configure.ac`, `Makefile.am`, `autogen.sh`)
- **References**: the modernization plan/01

## Context

retrace has used Autotools since 2017. The `configure.ac` is 286 lines with
130+ lines of `AC_CHECK_DECLS`/`AC_CHECK_TYPES` probes for features that are
universally available on every platform retrace still supports. Modern
packaging channels (vcpkg, conan, system package managers) all expect CMake.
IDE integration is dramatically better with CMake.

The current GHA workflows target `ubuntu-18.04` and `macos-10.15` runners
that have been removed from GitHub Actions — the CI is broken before any
modernization even starts.

## Decision

Migrate to CMake as the primary build system. Both build systems coexist
during Phase 1-2; Autotools is removed in Phase 3 (TODO 01 acceptance
criteria).

CMake target:

- Minimum version: CMake 3.20 (matches Ubuntu 22.04 system cmake).
- Default generator: Ninja.
- Default C standard: C11.
- Exported targets via `install(EXPORT ...)`.
- vcpkg manifest mode for dependencies.

## Alternatives considered

- **Meson**: faster than CMake, but vcpkg/conan integration is weaker, and
  downstream consumers (the library use case) overwhelmingly expect CMake.
- **Bazel**: overkill for a C library; not idiomatic in the C ecosystem.
- **Stay on Autotools forever**: would require resurrecting dead runners
  in CI; downstream packaging would remain painful.

## Consequences

- **Positive**: modern IDE support, native cross-compilation, vcpkg/conan
  integration, single source of truth for build flags.
- **Negative**: two build systems in tree during transition (Autotools files
  remain gitignored-but-present until Phase 3).
- **Neutral**: contributors must learn CMake if they don't already know it.
