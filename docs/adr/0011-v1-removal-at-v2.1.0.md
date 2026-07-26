# ADR-0011: Remove v1 source at v2.1.0

- **Status**: accepted
- **Date**: 2026-07-26
- **Supersedes**: ADR-0005

## Context

ADR-0005 set v1 on a deprecation path with removal at v3.0. Once v2
reaches feature parity on every v1 platform, there is no factual
justification for keeping v1.

## Decision

Remove the entire src/v1/ source tree at v2.1.0.

## Rationale

No unique platform coverage remains. Text config source preserves v1
config syntax. Migration tool converts complex configs. Distribution
shrinks ~50%.
