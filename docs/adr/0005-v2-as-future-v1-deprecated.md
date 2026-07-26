# ADR-0005: v2 as future, v1 deprecated

- **Status**: accepted
- **Date**: 2026-07-26
- **References**: TODO.roadmap/10

## Context

v1 (per-function C wrappers) shipped in 2017. v2 (assembly-driven engine)
is the active development direction since 2017 but never reached feature
parity with v1 in the docs. Both ship today.

Maintaining both is a tax: every new feature must be considered for both
codepaths. v2's architecture is strictly better (model-driven, plugin-based,
JSON-driven actions).

## Decision

v2 is the future. v1 enters a deprecation cycle:

- **Phase 1** (immediate): v1 still ships by default. README has a deprecation
  notice. Migration tool (`retrace migrate v1-to-json`) ships.
- **Phase 2** (after new CLI lands): v1 build emits deprecation warning. v1
  binary emits a one-line notice on every run.
- **Phase 3** (v2.1): v1 build is disabled by default.
- **Phase 4** (v3.0, no earlier than 2027-Q1): v1 source removed.

The v1 line-config syntax is preserved via the "text" config source
(TODO 04), so existing user configs continue to work against the new engine
even after v1 source removal.

## Alternatives considered

- **Keep v1 indefinitely**: ongoing maintenance tax; v2 never fully replaces.
- **Hard cutover**: breaks every existing v1 user. Unacceptable.
- **Rewrite v1 as a thin compatibility layer over v2** (Phase 1): tempting
  but the syntax translation isn't 1:1 for every directive. Easier to keep
  v1 source for one more release cycle and rely on the migration tool.

## Consequences

- **Positive**: clear end-state; v1 users have a path; v2 gets all new features.
- **Negative**: v1 source stays in tree for ~6 months longer than strictly
  necessary.
- **Risk**: deprecation window too short — extended if any user reports
  migration blockers.
- **Hard rule**: per the global "never delete source files" rule, `src/v1/`
  is removed only after (a) the text config source has 100% v1 syntax coverage,
  (b) the migration tool produces equivalent scripts for every example, and
  (c) explicit user sign-off in a dedicated PR.
