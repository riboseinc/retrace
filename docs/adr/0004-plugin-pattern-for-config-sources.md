# ADR-0004: Plugin pattern for config sources

- **Status**: accepted
- **Date**: 2026-07-26
- **References**: the modernization plan/04

## Context

retrace has two parallel config formats:

- v1: line-oriented text, parsed in `src/v1/common.c::rtr_get_config_*`.
- v2: JSON, parsed in `src/v2/conf.c` using vendored `parson`.

The engine knows about both. There's no way to build a script
programmatically (e.g., a test framework that wants to inject malloc
failures must write JSON to a temp file).

## Decision

Define a single `retrace_config_source_t` interface with `parse_file` and
optional `parse_buffer` callbacks. Each source (json, text, future toml)
self-registers. All sources produce the same `retrace_script_t` value
object — the engine never sees JSON or text.

Additionally, expose a programmatic builder API
(`retrace_script_new` / `_add_intercept` / `_add_action`) as the "api"
config source. This is the foundation for the "library as docker layer"
use case: embeddings build scripts in-process without serialization.

## Alternatives considered

- **Single canonical format (JSON only)**: simpler, but breaks every
  existing v1 config — unacceptable migration cost.
- **Hand-coded builder only, deprecate file formats**: would force every
  user to write code — too high a barrier for ops / security tooling.

## Consequences

- **Positive**: same script regardless of source format; programmatic
  embedding is first-class; new formats (toml, yaml) are additive.
- **Negative**: parser code is duplicated across sources (JSON parser
  stays vendored; text parser stays in C). Mitigated by shared validator
  that runs on the resulting script.
- **Migration**: every v1 example config is converted to an equivalent
  v2 JSON via the migration tool (TODO 10) and tested for equivalence.
