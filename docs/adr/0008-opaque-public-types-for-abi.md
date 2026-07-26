# ADR-0008: Opaque public types for ABI stability

- **Status**: accepted
- **Date**: 2026-07-26
- **References**: the modernization plan/05

## Context

The public C API (TODO 05) must be ABI-stable across minor versions (per
ADR-0006). If callers can see struct layouts, any field reordering or
addition breaks ABI — even when the API itself is unchanged.

The current `src/v2/engine.h` exposes `struct ThreadContext` and
`struct FuncPrototype` definitions to anyone who includes the header.

## Decision

All public types are opaque handles — forward-declared in
`include/retrace/retrace.h` as `typedef struct retrace_engine retrace_engine_t;`,
with the struct definition living only in `src/core/internal/*.h` (never
installed).

All access goes through functions:

```c
retrace_engine_t *eng;
retrace_engine_create(&eng);
retrace_engine_set_script(eng, script);
/* caller never sees sizeof(retrace_engine_t) */
```

Visibility annotations enforce this:

- Public functions: `__attribute__((visibility("default")))`
- Internal everything else: `__attribute__((visibility("hidden")))`

CMake sets `CMAKE_C_VISIBILITY_PRESET hidden` and
`CMAKE_VISIBILITY_INLINES_HIDDEN ON` globally; only functions tagged
`RETRACE_API` are exported.

ELF symbol versioning via `cmake/retrace.exports.map` ties each public
symbol to a minimum MAJOR version.

## Alternatives considered

- **Public struct definitions with versioned fields**: brittle, callers will
  reach into fields anyway, locks us into struct layout forever.
- **C++ with pimpl**: would require C++ runtime in every consumer; rejected
  for a C library.
- **C ABI with concrete struct + version field**: callers must initialize
  correctly; common in glibc but error-prone.

## Consequences

- **Positive**: struct layout can change freely between MINOR versions;
  callers cannot reach into private state (enforces "no private bypass" rule
  from the global CLAUDE.md); ABI stability is achievable.
- **Negative**: every field access is a function call — slight performance
  cost. Hot paths (per-call dispatch) use thread-local context internally;
  public API call cost is negligible relative to a libc call.
- **Trade-off**: opaque types require callers to use accessors — but this is
  the cost of a stable library API.
