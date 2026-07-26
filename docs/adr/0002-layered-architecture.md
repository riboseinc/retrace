# ADR-0002: Layered architecture

- **Status**: accepted
- **Date**: 2026-07-26
- **References**: TODO.roadmap/00, TODO.roadmap/02

## Context

The current `src/v2/` mixes concerns: `engine.c` knows about JSON parsing,
`conf.c` knows about engine internals, `arch/x86-64/` assembly bleeds into
the engine via `void *arch_spec_ctx`. This blocks:

- Adding non-JSON config sources.
- Adding non-assembly backends (ptrace, eBPF, Frida, Windows DLL injection).
- Embedding retrace as a library (no programmatic script construction).

## Decision

Adopt a strict 7-layer architecture where dependencies flow only downward:

```
Layer 7 — Distribution (packaging)
Layer 6 — CLI
Layer 5 — Public library API
Layer 4 — Config sources
Layer 3 — Backends
Layer 2 — Core domain model
Layer 1 — Foundation (build, repo)
```

Each layer is a CMake target. Each layer may only `#include` from itself or
lower layers. Cross-layer references upward are a build error (enforced via
include-path discipline; future clang-tidy check could enforce in CI).

## Alternatives considered

- **Monolithic library with namespaced prefixes** (e.g., `retrace_engine_*`,
  `retrace_json_*`): keeps everything visible, easier refactoring, but
  blurs module boundaries. Rejected — boundaries need enforcement.
- **Microservices**: not applicable — retrace is a single-process library.

## Consequences

- **Positive**: each concern has one home (MECE); new backends/config sources
  are purely additive (OCP); public API is small and explicit.
- **Negative**: more files, more CMake targets — slight complexity increase.
- **Mitigation**: the layering is documented in TODO.roadmap/00 and the
  architecture document (TODO 11).
