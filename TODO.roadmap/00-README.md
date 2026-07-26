# retrace Modernization Roadmap

This directory is the canonical plan for modernizing retrace from a 2017-era
Autotools C project into a layered, library-first, CLI-driven, multi-platform
tool. Each file is MECE-scoped: it owns exactly one architectural concern and
does not overlap with siblings.

## How to use this roadmap

- Each `NN-*.md` file is a work item. The status legend below is at the top of every file.
- Dependencies flow strictly downward (lower numbers don't import higher numbers).
- Code tasks within a file are tagged `[P0]` (blocker), `[P1]` (release-critical),
  `[P2]` (post-release), `[P3]` (nice-to-have).
- Each task has a checkbox. Update on completion.

## Status legend

- `[ ]` — pending
- `[~]` — in progress / partial
- `[x]` — done
- `[!]` — blocked (see notes)

## Layer map (architectural dependency direction)

```
┌─────────────────────────────────────────────────────────────────┐
│ Layer 7 — Distribution: vcpkg, conan, system pkgs (TODO 07)     │
├─────────────────────────────────────────────────────────────────┤
│ Layer 6 — CLI: retrace binary, subcommands, manpages (TODO 06)  │
├─────────────────────────────────────────────────────────────────┤
│ Layer 5 — Public library API: <retrace/retrace.h> (TODO 05)     │
├─────────────────────────────────────────────────────────────────┤
│ Layer 4 — Config sources: JSON, text, programmatic (TODO 04)    │
│ Layer 3 — Backends: LD_PRELOAD, DYLD, ptrace, eBPF (TODO 03)    │
├─────────────────────────────────────────────────────────────────┤
│ Layer 2 — Core domain model: engine, script, action (TODO 02)   │
├─────────────────────────────────────────────────────────────────┤
│ Layer 1 — Foundation: CMake build, repo hygiene (TODO 01)       │
└─────────────────────────────────────────────────────────────────┘

Cross-cutting:
  TODO 08 — testing/quality (touches all layers)
  TODO 09 — CI/CD (touches all layers, one workflow per platform)
  TODO 10 — v1 → v2 migration
  TODO 11 — documentation
  TODO 12 — architecture decision records
```

## Index

| # | Title | Status | Phase |
|---|-------|--------|-------|
| 01 | [Foundation: CMake build, repo hygiene](01-foundation-build-cmake.md) | [~] | 1 |
| 02 | [Core domain model](02-core-domain-model.md) | [ ] | 1 |
| 03 | [Backend plugin system](03-backends-plugin-system.md) | [ ] | 2 |
| 04 | [Config source abstraction](04-config-sources.md) | [ ] | 2 |
| 05 | [Public library API](05-library-public-api.md) | [~] | 2 |
| 06 | [CLI redesign](06-cli-redesign.md) | [ ] | 3 |
| 07 | [Packaging & distribution](07-packaging-distribution.md) | [~] | 3 |
| 08 | [Testing & quality](08-testing-quality.md) | [ ] | 1→4 |
| 09 | [CI/CD modernization](09-ci-cd-modernization.md) | [~] | 1→4 |
| 10 | [v1 deprecation & migration](10-migration-v1-deprecation.md) | [ ] | 4 |
| 11 | [Documentation](11-documentation.md) | [ ] | 4 |
| 12 | [Architecture Decision Records](12-architecture-decisions.md) | [~] | 1 |

## Phasing

**Phase 1 — Foundation (P0, weeks 1-2)**: TODOs 01, 02, 12-ADRs. Land CMake
build parallel to Autotools, capture the target architecture in ADRs, refactor
v2 internals to match the model in TODO 02. Nothing user-facing changes yet.

**Phase 2 — Library surface (P1, weeks 3-6)**: TODOs 03, 04, 05. Extract
backends and config sources into plugins behind a public API. The library is
now embeddable.

**Phase 3 — Product (P1, weeks 7-10)**: TODOs 06, 07. New `retrace` CLI with
subcommands. vcpkg port, conan recipe, system packages.

**Phase 4 — Hygiene (P2, ongoing)**: TODOs 08, 09, 10, 11. Test pyramid,
modern multi-platform CI (one workflow per OS × arch, mirroring the
libemf2svg pattern), v1 deprecation path, doxygen + user guide.

## Global design constraints

These constraints apply to every code change in this roadmap:

1. **Open/Closed Principle.** Adding a new backend, config source, action, or
   intercepted function is purely additive (one new file, one registration
   call). Never modify the engine to add a backend.
2. **MECE.** Each concern lives in exactly one module. No two modules own the
   same responsibility. No gap in responsibility coverage.
3. **DRY without premature abstraction.** Three near-identical lines is fine;
   three near-identical functions is a sign to extract.
4. **Model-driven, semantically-driven.** File names, struct names, function
   names match domain concepts (engine, script, action, prototype). Method
   names match domain actions (`script_add_intercept`, not `add_to_list`).
5. **Performance.** Hot paths (per-call dispatch) avoid allocations, avoid
   locks where possible, use atomic-builtins for the reentrancy guard. The
   engine must not be measurable on a no-op intercept.
6. **No private-method bypass.** In C: do not reach into another module's
   `static` state via `extern`, do not `#define private public`-style hacks,
   do not call `static` functions from outside their file. If you need the
   data, expose it through a declared accessor.
7. **No `instance_variable_set/get`-equivalent.** In C: do not read or write
   another module's static globals. State crosses module boundaries only via
   the public API on opaque structs.
8. **No `respond_to?`-equivalent.** In C: do not do runtime type-tag checking
   when compile-time polymorphism (function pointers in `struct retrace_action`
   etc.) does the job. Use `is_a`-equivalents (a typed enum on a tagged union)
   only when polymorphism via vtable does not fit.
9. **No `require_relative`-equivalent.** In C: forward-declare in headers, use
   include guards, and let the linker resolve. Internal headers go in
   `src/<module>/`, public headers in `include/retrace/`. Optional deps load
   via `dlopen`/`LoadLibrary` at runtime — no compile-time link to plugins.
10. **Specs throughout.** Every public function has a cmocka unit test. Every
    backend has an integration test. Every config format has golden-file
    round-trip tests.

## What this roadmap explicitly does NOT do

- Delete v1 source code. v1 is deprecated in TODO 10 but not removed until the
  migration tool has 100% coverage of v1 config syntax. (Global rule: never
  delete source files.)
- Replace the Autotools build until the CMake build has feature parity on every
  platform the current CI matrix covers. Both build systems coexist during
  Phase 1-2; Autotools is removed in Phase 3.
- Push tags, push to main, or merge without explicit PR. All work in this
  roadmap goes through PRs on feature branches.
- Add AI attribution to any commit message. (Global rule.)
