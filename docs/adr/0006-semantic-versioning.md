# ADR-0006: Semantic versioning

- **Status**: accepted
- **Date**: 2026-07-26
- **References**: TODO.roadmap/05, TODO.roadmap/07

## Context

retrace's `configure.ac` is at version `2.0.1`. There's no documented
versioning policy. The library is about to ship a public C API (TODO 05)
where ABI stability matters.

## Decision

Adopt Semantic Versioning 2.0 (https://semver.org/).

- `MAJOR`: incremented on incompatible API/ABI changes (e.g., struct field
  removal, function signature change, behavior change that breaks working code).
- `MINOR`: incremented on backward-compatible feature additions (new function,
  new struct field at end, new default behavior opt-in).
- `PATCH`: incremented on backward-compatible bug fixes.

The `RETRACE_VERSION_MAJOR`/`_MINOR`/`_PATCH` macros in
`include/retrace/version.h` are the canonical source; CMake's `project(VERSION)`
reads from them.

The shared library soname follows: `libretrace.so.MAJOR` (e.g.,
`libretrace.so.2`), with versioned symbol scripts (`retrace.exports.map`)
tying symbol visibility to a minimum MAJOR.

Tags: `v2.0.0`, `v2.0.1`, etc. Branch: `release/2.0.x` for patch series.

## Alternatives considered

- **Calendar versioning** (`2026.7`): misleading — implies time-based
  releases, but retrace ships when ready.
- **No version policy**: today's state — unacceptable for a library.

## Consequences

- **Positive**: downstream consumers can pin and upgrade safely; vcpkg/conan
  expect semver.
- **Negative**: discipline required — every public-API change must bump MAJOR.
- **Mitigation**: ADR-0008 (opaque types) minimizes the surface where ABI
  changes are even possible.
