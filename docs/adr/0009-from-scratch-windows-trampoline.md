# ADR-0009: From-scratch Windows trampoline (no MinHook, no Detours)

- **Status**: accepted
- **Date**: 2026-07-26

## Context

retrace needs native Windows support. Windows has no LD_PRELOAD; the only
mechanism for libc interposition is inline hooking.

## Decision

Implement the Windows trampoline from scratch. No Detours (commercial),
no MinHook (BSD-3 attribution).

## Rationale

BSD-2 license purity. retrace makes a "no surprises" promise to consumers;
vendoring third-party hooking libraries weakens that.

## Consequences

1-2 weeks of additional implementation effort. Conservative refuse-to-hook
policy narrows the supported function set in v1.
