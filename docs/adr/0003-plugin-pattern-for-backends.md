# ADR-0003: Plugin pattern for backends

- **Status**: accepted
- **Date**: 2026-07-26
- **References**: the modernization plan/03

## Context

retrace currently supports one interposition strategy per platform: Linux
uses `LD_PRELOAD`, macOS uses `DYLD_INSERT_LIBRARIES`, BSDs use `RTLD_NEXT`.
These are baked into the build (per-arch assembly trampolines in
`src/v2/arch/x86-64/`) and the launcher (`setenv("LD_PRELOAD", ...)` in
`src/v2/retrace_v2.c::fork_cmd`).

This blocks:

- Windows support (no LD_PRELOAD).
- ptrace-based tracing of statically-linked binaries.
- eBPF-based kernel-visible tracing.
- Runtime selection of backend based on target characteristics.

## Decision

Define a single `retrace_backend_t` interface with `probe`, `spawn`,
`attach`, `detach`, `translate_frame` callbacks. Backends self-register via
a constructor that calls `retrace_backend_register(&backend)`. The engine
selects a backend by probing all registered backends and picking the
highest-rank one that returns 1 from `probe()`.

Each backend lives in its own subdirectory under `src/backends/` and is a
separate CMake target. The current x86-64 assembly trampolines are
extracted unchanged into `preload-elf`, `preload-macho`, `preload-bsd`
subdirectories.

## Alternatives considered

- **Compile-time single backend** (one binary per backend): simpler but
  explodes the binary matrix; users have to know which to download.
- **Runtime-loaded `dlopen` plugins**: maximal flexibility but adds a plugin
  search-path problem. Rejected for v2.0; can layer on top later if needed.

## Consequences

- **Positive**: backends are MECE-partitioned; new backends are additive
  (OCP); engine has zero knowledge of specific interposition mechanisms.
- **Negative**: backend interface must be stable across versions (otherwise
  ABI breaks). Mitigated by opaque types and vtable.
- **Risk**: a backend may want capabilities the interface doesn't expose
  (e.g., kernel-assisted uprobe registration). Mitigation: interface can
  grow optional callbacks with default no-op implementations.
