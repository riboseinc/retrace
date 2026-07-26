# Architecture Decision Records

Each ADR captures one architectural decision, the alternatives considered,
and the consequences. ADRs are immutable once merged — supersession happens
via a new ADR that references the old.

## Index

| # | Title | Status |
|---|-------|--------|
| [0001](0001-cmake-as-primary-build.md) | CMake as primary build system | accepted |
| [0002](0002-layered-architecture.md) | Layered architecture (7 layers) | accepted |
| [0003](0003-plugin-pattern-for-backends.md) | Plugin pattern for backends | accepted |
| [0004](0004-plugin-pattern-for-config-sources.md) | Plugin pattern for config sources | accepted |
| [0005](0005-v2-as-future-v1-deprecated.md) | v2 as future, v1 deprecated | accepted |
| [0006](0006-semantic-versioning.md) | Semantic versioning | accepted |
| [0007](0007-json-as-canonical-config.md) | JSON as canonical config format | accepted |
| [0008](0008-opaque-public-types-for-abi.md) | Opaque public types for ABI stability | accepted |

## When to write an ADR

Write an ADR when making a decision that:

- Is hard to reverse (would require a major version bump, or affects many
  files).
- Has multiple reasonable alternatives.
- Will be questioned by future contributors ("why did they do it this way?").

Don't write an ADR for: bug fixes, refactors, new tests, dependency bumps.

## Template

```markdown
# ADR-NNNN: <imperative title>

- **Status**: proposed | accepted | rejected | deprecated | superseded by ADR-MMMM
- **Date**: YYYY-MM-DD
- **References**: TODO.roadmap files, related ADRs

## Context
<problem statement, current state, forces>

## Decision
<the chosen option, stated imperatively>

## Alternatives considered
<each alternative, with rejection rationale>

## Consequences
<positive, negative, neutral; mitigations for negatives>
```
