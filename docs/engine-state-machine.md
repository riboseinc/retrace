# Engine state machine

The lifecycle of a single intercepted call, from the asm trampoline
through the engine back to the caller. This doc explains the order
of operations in `retrace_engine_wrapper` and the role of each
MECE module (per ADR-0013).

## Big picture

```
caller             asm trampoline      engine.c            actions            real libc
  |                      |                |                   |                   |
  | -- call open() ----> |                |                   |                   |
  |                      | -- push frame, |                   |                   |
  |                      |   bl engine -->|                   |                   |
  |                      |   _wrapper     |                   |                   |
  |                      |                |                   |                   |
  |                      |                +-- get real_impl (dlsym RTLD_NEXT)      |
  |                      |                |                   |                   |
  |                      |                +-- if not inited: sched real, return   |
  |                      |                |                                       |
  |                      |                +-- acquire ThreadContext (per-thread)  |
  |                      |                |                                       |
  |                      |                +-- if real_impl is NULL:               |
  |                      |                |       set ret_val=-1, return          |
  |                      |                |                                       |
  |                      |                +-- sched real (default)                |
  |                      |                |                                       |
  |                      |                +-- if guard_active(ctx): return        |
  |                      |                |   (nested call -- avoid recursion)    |
  |                      |                |                                       |
  |                      |                +-- guard_enter(ctx, real_impl, arch)   |
  |                      |                |                                       |
  |                      |                +-- look up prototype                  |
  |                      |                |       if missing: goto cleanup        |
  |                      |                |                                       |
  |                      |                +-- log-filter check                   |
  |                      |                |       if filtered: goto cleanup      |
  |                      |                |                                       |
  |                      |                +-- setup_params from frame            |
  |                      |                |       if fails: goto cleanup         |
  |                      |                |                                       |
  |                      |                +-- find intercept_script              |
  |                      |                |       if none: goto cleanup          |
  |                      |                |                                       |
  |                      |                +-- cancel default sched_real          |
  |                      |                |                                       |
  |                      |                +-- action_runner_run(actions[]) -----+|
  |                      |                |                   |                   ||
  |                      |                |                   +-- log_params --->||
  |                      |                |                   |                   ||
  |                      |                |                   +-- call_real ----->|----> real open()
  |                      |                |                   |                   ||<-- ret
  |                      |                |                   |                   ||
  |                      |                |                   +-- modify_ret -->  ||
  |                      |                |                   |                   ||
  |                      |                |                   <-- return -----------||
  |                      |                |                                       |
  |                      |                +-- set_ret_val(ctx->ret_val)           |
  |                      |                |                                       |
  |                      |                +-- cleanup: ctx_clear (free params)    |
  |                      |                |                                       |
  |                      | <-- ret -------+                                       |
  | <--- ret ------------+                                               |
```

## MECE module responsibilities

| Module | Responsibility | Why separate |
|--------|----------------|--------------|
| `engine.c` | Orchestrator. Calls the others in strict order. | Single point of dispatch; no business logic. |
| `thread_context.c` | Per-thread state lifecycle (alloc, register, destroy on thread exit). | Runs once per thread, not per call. |
| `reentrance_guard.c` | In-use marker (`active` / `enter`). | Distinguishes "what is the real impl?" from "is this thread already inside an intercept?" -- previously overloaded on `real_impl != NULL`. |
| `cleanup.c` | Post-intercept reset (free param buffers + zero context). | Runs after every call. Different cadence from lifecycle. |
| `script_resolver.c` | Find the matching `intercept_script` for a given func_name. | Isolates JSON-walking from engine orchestration. |
| `action_runner.c` | Iterate the script's actions array, dispatch each via the registry. | Isolates action-loop semantics (abort on -1, logging) from engine. |

## State machine (formal)

The engine transitions through these states per call:

1. **ENTER** -- trampoline called `retrace_engine_wrapper(func_name, arch_spec_ctx)`.
2. **REAL_IMPL_LOOKUP** -- `retrace_as_get_real_safe(func_name)` returns the real libc pointer (or NULL).
3. **INIT_CHECK** -- if `!retrace_inited`, schedule real call and return. Skip everything else.
4. **CTX_ACQUIRE** -- `retrace_thread_context_get()` returns the per-thread `ThreadContext*` (allocating one if first call on this thread).
5. **REAL_IMPL_NULL_CHECK** -- if real_impl is NULL, set `ret_val=-1` and return. (For CRT funcs; caller probably crashes anyway.)
6. **DEFAULT_REAL_SCHED** -- `retrace_as_sched_real(arch_spec_ctx, real_impl)`. Marks the trampoline to call real by default.
7. **GUARD_CHECK** -- `retrace_reentrance_guard_active(ctx)`. If true: nested call from inside an action, return early (real will run via the sched above).
8. **GUARD_ENTER** -- `retrace_reentrance_guard_enter(ctx, real_impl, arch_spec_ctx)`. Mark active.
9. **PROTO_LOOKUP** -- `retrace_func_get(func_name)`. If NULL: `goto cleanup`.
10. **LOG_FILTER** -- `retrace_logger_func_loggable(func_name)`. If false: `goto cleanup` (real still runs).
11. **SETUP_PARAMS** -- `retrace_as_setup_params(arch_spec_ctx, proto, params, &cnt)`. If fails: `goto cleanup`.
12. **SCRIPT_LOOKUP** -- `retrace_script_find(intercept_scripts, func_name, ret_addr)`. If NULL: `goto cleanup`.
13. **CANCEL_DEFAULT_REAL** -- `retrace_as_cancel_sched_real(arch_spec_ctx)`. Real won't run unless `call_real` action re-schedules it.
14. **ACTION_RUN** -- `retrace_action_runner_run(ctx, func_name, script)`. Walks the actions array.
15. **RETVAL_WRITEBACK** -- `retrace_as_set_ret_val(arch_spec_ctx, ctx->ret_val)`.
16. **CLEANUP** -- `retrace_thread_context_clear(ctx)`. Frees any param buffers actions allocated; zeroes ctx for next call.

States 1-7 are pre-flight (always run, no actions yet). States 8-14 are the intercept body. States 15-16 are post-intercept.

## Reentrance handling

The guard is critical because any libc call inside an action (e.g. `log_info -> fprintf -> malloc`) is itself intercepted. Without the guard, the recursion is unbounded.

The guard works because:
- `real_impl` is set on enter (step 8).
- A nested call from inside an action reacquires the same `ThreadContext` (per-thread), sees `guard_active`, bails.
- `cleanup` zeroes `real_impl` after the outer action completes.

Without the per-thread `ThreadContext`, two threads could clobber each other's guard state.

## Failure modes

| Failure | Engine response |
|---------|-----------------|
| Init never completed | Schedule real, return. No intercept. |
| `real_impl == NULL` (no libc symbol) | Set `ret_val=-1`, return. |
| `ThreadContext` alloc fails | Log error, return. Real does not run (sched was set). |
| Prototype missing | Skip actions, real runs. |
| Log filter excludes function | Skip actions, real runs. |
| `setup_params` fails | Skip actions, real runs. |
| No matching script | Skip actions, real runs. |
| Action returns non-zero | `action_runner` aborts script, real does not run (default was cancelled). |
| Param buffer alloc inside action fails | Action handles it; typically logs + continues. |

## See also

- `src/core/engine.c` -- the orchestrator (state machine implementation)
- `docs/adr/0013-engine-mece-split.md` -- the architectural decision
- `docs/architecture.md` -- higher-level architecture overview
- `TODO.complete/13-engine-mece-refactor.md` -- the tactical plan that drove the split
