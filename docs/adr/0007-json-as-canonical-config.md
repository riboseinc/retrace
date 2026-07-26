# ADR-0007: JSON as canonical config format

- **Status**: accepted
- **Date**: 2026-07-26
- **References**: TODO.roadmap/04

## Context

retrace v2's config is JSON. v1's is line-oriented text. The modernization
needs a single canonical serialization for round-tripping, distribution,
and programmatic generation.

## Decision

JSON is the canonical, stable, machine-readable format for retrace scripts.
All other formats (text, toml, programmatic) are inputs that get converted
to the canonical JSON representation internally — though the engine itself
never sees JSON, only the in-memory `retrace_script_t`.

The JSON schema is documented (TODO 11) and versioned via a
`schema_version` field at the root. Schema evolution rules:

- Adding a new field: backward compatible.
- Removing a field: MAJOR version bump.
- Renaming a field: MAJOR version bump.

Text format remains as a thin "human-friendly" alias for simple cases. The
migration tool (TODO 10) is the only supported v1→v2 path.

## Alternatives considered

- **TOML**: more pleasant for hand-authoring, but JSON has universal parser
  support and is what v2 already uses. TOML can ship later as an alternate
  config source (TODO 04 extension).
- **YAML**: significant whitespace causes endless pain in C tooling.
- **XML**: too verbose, requires heavy parser deps.
- **Custom DSL**: maximally expressive, maximally unsupported.

## Consequences

- **Positive**: zero migration cost for existing v2 users; universal tooling.
- **Negative**: JSON is verbose for hand-authoring — mitigated by the text
  format alias for simple cases.
- **Mitigation**: schema is documented and round-trip tested (TODO 08
  golden-file tests).
