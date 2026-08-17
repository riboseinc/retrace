# ADR-0014: The public API matches the implementation

- Status: Accepted
- Date: 2026-08-17
- Deciders: retrace maintainers

## Context

`include/retrace/retrace.h` has declared a rich public API since
the v2.0.0 modernization plan: an engine lifecycle
(`retrace_engine_create`/`destroy`), a programmatic script
builder (`retrace_script_*`, `retrace_action_params_*`), config
parsing entry points (`retrace_config_*`), introspection
(`retrace_engine_list_prototypes`/`describe_*`), and error
reporting (`retrace_last_error*`).

An audit before v2.5.0 (`nm -g` on the built library) showed that
of the ~28 declared `RETRACE_API` functions, **only the two added
in v2.4.0 actually existed** (`retrace_attach_process`,
`retrace_list_backends`) — and even `retrace_version()` had no
definition. The rest were scaffold that never landed.

The header also claimed "ABI-stable from v2.0.0". In practice a
consumer who wrote against the header compiled successfully and
failed at link time — the worst failure mode for a public
interface: it lies at exactly the point where the lie is most
expensive to discover.

## Decision

1. **The public header declares only implemented symbols.** The
   never-implemented declarations are removed. The shipped
   surface is: the status codes, the version macros
   (`include/retrace/version.h`), `retrace_version()`,
   `retrace_version_info()` (newly implemented), and the v2.4.0
   attach/list-backends pair.
2. **A guard test enforces the contract.**
   `test/unit/test_public_api.c` dlsyms every symbol the header
   declares and fails the build if any is missing. Adding a
   declaration without a definition can no longer ship silently.
3. **New API lands implementation-first.** When the engine-object
   and script-builder APIs are designed, each function is added
   to the header in the same change that defines it — the guard
   test makes anything else a build failure.

## Consequences

- Consumers compiling against the old header see compile errors
  on the removed names instead of link errors later. Since the
  symbols never existed in any linkable build, no working program
  can regress; this is a defect fix, not a breaking change.
- The header shrinks from ~260 lines of partly-fictional API to a
  small, fully-real surface — the honest core of what the library
  guarantees today.
- The re-introduction path for the richer API is unchanged
  (engine objects, script builder, config sources per the
  original plan) but is now gated on real implementations, and
  each addition will carry its own tests at the public boundary.

## Alternatives considered

- **Implement the full declared surface now.** Requires the
  engine-object redesign (the engine is process-global today, not
  a handle) plus script/params/config object lifetimes — a large
  design effort that should not block making the header truthful.
- **Keep the declarations with a "not yet implemented" note.**
  Rejected: the failure mode (compile-then-link-fail) remains,
  and tooling that reads headers (language bindings, SWIG, IDEs)
  cannot see the comment.
