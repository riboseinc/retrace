# 12 — Architecture Decision Records (ADRs)

**Status**: [~] in progress (initial ADRs written)
**Layer**: cross-cutting (records, doesn't ship code)
**Depends on**: 01 (so the ADR can reference the chosen build system)
**Blocks**: nothing (records for future reference)

## Goal

Capture every architectural decision with context, alternatives considered,
and consequences. ADRs are immutable once merged — supersession happens via a
new ADR that references the old one.

Format: Michael Nygard's ADR template, one file per decision, numbered.

## File layout

```
docs/adr/
├── 0000-template.md                          # template for new ADRs
├── 0001-cmake-as-primary-build.md           # written
├── 0002-layered-architecture.md             # written
├── 0003-plugin-pattern-for-backends.md      # written
├── 0004-plugin-pattern-for-config-sources.md # written
├── 0005-v2-as-future-v1-deprecated.md       # written
├── 0006-semantic-versioning.md              # written
├── 0007-json-as-canonical-config.md         # written
├── 0008-opaque-public-types-for-abi.md      # written
└── README.md                                 # index
```

## Tasks

### [P0] Initial ADR set (all written in this turn)
- [x] ADR-0001: CMake as primary build
- [x] ADR-0002: Layered architecture (core / backends / config / api / cli)
- [x] ADR-0003: Plugin pattern for backends
- [x] ADR-0004: Plugin pattern for config sources
- [x] ADR-0005: v2 as future, v1 deprecated
- [x] ADR-0006: Semantic versioning
- [x] ADR-0007: JSON as canonical config format
- [x] ADR-0008: Opaque public types for ABI stability

### [P1] Future ADRs (write as decisions are made)
- [ ] ADR-0009: vcpkg manifest mode (TODO 07)
- [ ] ADR-0010: ccache + Ninja defaults (TODO 01)
- [ ] ADR-0011: Symbol versioning approach (TODO 05)
- [ ] ADR-0012: When to drop Autotools (Phase 3)
- [ ] ADR-0013: ASAN/UBSAN/TSAN in CI (TODO 08/09)
- [ ] ADR-0014: Interactive CLI mechanism (PTS vs alternatives)

## Acceptance criteria

- Every architectural choice referenced in TODOs 01-11 has a corresponding ADR.
- Each ADR is < 1 page and follows the template.
- New architectural decisions go through an ADR PR before code lands.
