# ADR-0010: AArch64 port supports float parameters from day one

- **Status**: accepted
- **Date**: 2026-07-26

## Context

The x86-64 trampoline does not support float/double function parameters
(known TODO in arch_spec_bottom.c). The AArch64 port needs to decide
whether to replicate the limitation or support floats from launch.

## Decision

AArch64 supports float parameters from day one. WrapperAArch64Frame saves
all 8 FP/SIMD argument registers (v0-v7) alongside the 8 integer arg
registers (x0-x7).

## Rationale

AArch64 PCS makes it cheap — the register files are orthogonal. Users
expect float support. Deliberate asymmetry is documented, not accidental.
