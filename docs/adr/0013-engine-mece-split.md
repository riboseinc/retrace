# ADR-0013: Engine MECE module split

- **Status**: accepted
- **Date**: 2026-07-30
- **Tracking issue**: #480

## Context

`src/core/engine.c` is 377 lines and mixes five distinct concerns in
its central dispatch function `retrace_engine_wrapper`:

1. Real-impl lookup (`retrace_as_get_real_safe`)
2. Reentrance guard (`retrace_inited` + `thread_ctx->real_impl` checks)
3. Thread-context lifecycle (`get_thread_context`, `clear_context`)
4. Script resolution (`get_i_script` — walks the JSON config)
5. Action dispatch (the `for` loop over `i_actions`)

Each concern is a single responsibility in the domain model, but
today they're tangled in one function with multiple `goto clean_up`
paths. Adding features (per-group log levels, action schemas, va_list
reconstruction for FP varargs — issue #503) is invasive because every
concern lives in the same scope.

## Decision

Split `engine.c` into MECE modules under `src/core/engine/`:

```
src/core/engine/
    engine.c              # retrace_engine_wrapper — the dispatcher
    thread_context.c      # per-thread state lifecycle
    reentrance_guard.c    # set/clear/check real_impl marker
    script_resolver.c     # find matching intercept_script
    action_runner.c       # iterate actions, dispatch via registry
```

Each module exposes a small header. `engine.c` just calls them in
order.

### Public surface unchanged

`retrace_engine_wrapper(func_name, arch_spec_ctx)` stays the only
public entry point. The asm trampolines call this function; their
contract doesn't change. The refactor is internal.

### Data model

`struct ThreadContext` becomes more clearly a value object:

```c
struct ThreadContext {
    /* Identity */
    const struct FuncPrototype *prototype;
    void *real_impl;

    /* Per-call state */
    struct FuncParam params[ENGINE_MAXCOUNT_PARAMS];
    int params_cnt;
    long ret_val;

    /* Engine bookkeeping */
    void *arch_spec_ctx;
    void *ret_addr;
};
```

The reentrance marker moves from `real_impl != NULL` (overloaded)
to an explicit `in_use` flag, so the guard reads as
`if (ctx->in_use) return early;` rather than inferring intent from
a pointer field.

## Rationale

- **MECE**: each concern owns exactly one file. No overlap.
- **OCP**: adding a new action behavior = new file in
  `actions/`; adding a new script-matching strategy = new helper in
  `script_resolver.c`. The dispatcher doesn't change.
- **Testability**: each module gets unit tests (issue #481 follow-up).
  Today testing the action loop requires spinning up the full engine.
- **DRY**: the `goto clean_up` pattern repeats cleanup logic; the
  split lets `action_runner.c` own cleanup in one place.

## Consequences

- One-time mechanical refactor: move code, extract headers, update
  `src/core/CMakeLists.txt`. Behavior is preserved — the same code
  runs in the same order, just from different files.
- The refactor lands as ONE PR with ctest green on every platform.
  No incremental merge: half-extracted state would be worse than
  the status quo.
- Slight increase in file count (5 files instead of 1). Worth it
  for the clarity.

## Non-goals

- Changing the dispatch order or the reentrance semantics.
- Making `ThreadContext` opaque (per ADR-0008) — it's an internal
  type, no ABI concern.
- Adding new features — those land as follow-up PRs once the
  refactor is in place.

## Open questions

- Should `reentrance_guard.c` own the `retrace_inited` global too,
  or stay focused on the per-thread marker? Lean: keep
  `retrace_inited` in `engine.c` since it's a process-wide gate,
  not a per-thread one.
