# 02 — Core domain model

**Status**: [ ] pending
**Layer**: 2 (core — every layer above depends on these types)
**Depends on**: 01
**Blocks**: 03, 04, 05

## Goal

Define the canonical in-memory model that captures what retrace *is*, independent
of how it's configured or how it gets installed into a target process. Refactor
the existing v2 engine, funcs, actions, data_types, and conf modules to expose
this model cleanly.

## Why

The current `src/v2/` is functional but tangled:

- `engine.c` knows about JSON (it parses `intercept_scripts` directly).
- `conf.c` is a JSON parser that knows about engine internals.
- `actions.c` is a registry that knows about every action's struct layout.
- `data_types.c` mixes type metadata with serialization.
- `funcs.c` mixes prototype metadata with the registration mechanism.

This makes it impossible to (a) add a non-JSON config source, (b) add a new
backend without touching the engine, or (c) embed retrace as a library where
the host application builds the script programmatically.

The fix is to define a small set of value types — **Engine**, **Script**,
**InterceptRule**, **Action**, **Prototype**, **ThreadContext** — each owning
exactly one responsibility, with the registries as MECE partitions of the
engine's state.

## Domain model

```c
/* Opaque handles (defined in src/core/, exposed via <retrace/retrace.h>) */

struct retrace_engine;          /* owns registries, lifecycle */
struct retrace_script;          /* immutable collection of intercept rules */
struct retrace_intercept_rule;  /* function selector + ordered actions */
struct retrace_action;          /* name + typed params + callback */
struct retrace_prototype;       /* function signature metadata */
struct retrace_thread_context;  /* per-invocation state (was: ThreadContext) */
struct retrace_param_value;     /* tagged union: int/string/ptr/struct/... */
```

### Engine

Owns:
- Prototype registry: `func_name → const retrace_prototype*`
- Action registry: `action_name → retrace_action_factory_fn`
- Backend handle (lazy, see TODO 03)
- Active script (atomic-swap)
- Per-thread context table (was: `pthread_key_t thread_ctx_key` in engine.c)
- Real-impl table (was: `struct RetraceRealImpls retrace_real_impls` — internal
  libc pointers, used to avoid reentrancy)

Does NOT own:
- Specific prototypes (those are declared in `src/core/prototypes/<header>.c`)
- Specific actions (those are declared in `src/core/actions/<name>.c`)
- Config parsing (that's TODO 04)
- Interposition mechanism (that's TODO 03)

### Script

An immutable, validated collection of intercept rules. Built by:
- A config source (TODO 04) parsing a file/string into a script.
- A programmatic builder API (TODO 05) constructing a script in C.

The script is validated against the engine's prototype registry at build time,
so the engine never has to do string lookups at intercept time.

### InterceptRule

One rule = (function selector, ordered action list).

The function selector is currently a glob (`"*"` or a bare name). Future
selectors: regex, prototype-shape match, address range. The selector is a
vtable so adding new selector kinds is purely additive.

### Action

A named callback. Signature (proposed):

```c
typedef enum {
    RETRACE_ACTION_OK = 0,
    RETRACE_ACTION_SKIP_CALL = 1,     /* don't call real impl */
    RETRACE_ACTION_HANDLED = 2,       /* call real impl with modified params */
    RETRACE_ACTION_ERROR = -1,
} retrace_action_result_t;

typedef retrace_action_result_t (*retrace_action_fn)(
    struct retrace_thread_context *ctx,
    const struct retrace_action_params *params);
```

Built-in actions (was: `src/v2/actions/basic.c`):
- `log_params` — serialize call to logger
- `call_real` — invoke real implementation
- `modify_in_param_str` — rewrite a string argument
- `modify_in_param_int` — rewrite an integer argument
- `modify_in_param_arr` — rewrite an array argument
- `modify_return_value_int` — synthesize return value
- `memory_fuzz` — was: `src/v2/actions/memfuzz.c`

Each action lives in its own file under `src/core/actions/`. Adding a new
action = new file + one registration call in `retrace_engine_init`. Engine
code never changes.

### Prototype

Function signature metadata, used to:
- Validate that a script's parameter references match the real function.
- Serialize parameters for `log_params`.
- Drive the assembly trampoline (which registers to save/restore).

Was: `struct FuncPrototype` in `src/v2/funcs.h`. Refactor moves the struct to
`src/core/prototype.h` and the per-header tables stay in
`src/core/prototypes/<header>.c` (currently `src/v2/prototypes/`).

### ThreadContext

Per-call state, allocated lazily per thread (was: `struct ThreadContext` in
`src/v2/engine.h`). Holds the prototype pointer, parsed params, return value
slot, and the architecture-specific stack frame pointer.

This is the only mutable state visible to an action callback. It is owned by
the engine, not by the action. Actions must not retain pointers into it past
their return.

## File layout (target)

```
src/core/
├── engine.h         / .c       — retrace_engine struct + lifecycle
├── script.h         / .c       — retrace_script + retrace_intercept_rule
├── action.h         / .c       — retrace_action + registry
├── prototype.h      / .c       — retrace_prototype + registry
├── thread_context.h / .c       — per-thread state
├── param_value.h    / .c       — tagged union for parameter values
├── real_impls.h     / .c       — was: src/v2/real_impls.{c,h}
├── prototypes/
│   ├── ctype.c                — was: src/v2/prototypes/ctype.c
│   ├── dirent.c
│   ├── locale.c
│   ├── signal.c
│   ├── stdio.c
│   ├── stdlib.c
│   ├── uio.c
│   └── unistd.c
└── actions/
    ├── log_params.c
    ├── call_real.c
    ├── modify_in_param_str.c
    ├── modify_in_param_int.c
    ├── modify_in_param_arr.c
    ├── modify_return_value_int.c
    └── memory_fuzz.c          — was: src/v2/actions/memfuzz.c
```

Each `.c` file is self-contained: declares its prototypes/actions at file scope
via `RETRACE_PROTOTYPE_DECLARE(...)` / `RETRACE_ACTION_REGISTER(...)` macros
that place entries in dedicated linker sections. The engine scans those
sections at init. Adding a function = adding a prototype entry — engine code
unchanged.

## Tasks

### [P0] Type definitions
- [ ] Define opaque struct handles in `include/retrace/internal/engine_types.h` (not the public API yet)
- [ ] Move `struct FuncPrototype` → `struct retrace_prototype` (rename + visibility)
- [ ] Move `struct ThreadContext` → `struct retrace_thread_context` (rename)
- [ ] Move `struct RetraceRealImpls` → `struct retrace_real_impls` (rename)
- [ ] Define `struct retrace_script`, `struct retrace_intercept_rule` (new)
- [ ] Define `struct retrace_action` with vtable-style factory (new)

### [P0] Registries as linker-section scans
- [ ] `RETRACE_PROTOTYPE_DECLARE(name, ...)` macro emits a `const struct retrace_prototype` into `__DATA,__retrace_protos` (Mach-O) / `__retrace_protos` (ELF section)
- [ ] `RETRACE_ACTION_REGISTER(name, fn)` macro emits a `const struct retrace_action` into `__retrace_acts`
- [ ] Engine init: walk both sections, build name-keyed hashtables
- [ ] Keep backward compat with existing `src/v2/funcs_symbols.S` during transition

### [P0] Engine lifecycle
- [ ] `retrace_engine_create(retrace_engine_t**)` — allocates, scans sections, builds registries
- [ ] `retrace_engine_destroy(retrace_engine_t*)`
- [ ] `retrace_engine_set_script(retrace_engine_t*, retrace_script_t*)` — atomic swap
- [ ] `retrace_engine_dispatch(retrace_engine_t*, const char* func_name, void* arch_frame)` — called by backends
- [ ] Constructor priority matches current `__attribute__((constructor(101)))` in `src/v2/main.c`

### [P1] Validation
- [ ] Script validates action parameter names against the prototype at load time, not at intercept time
- [ ] Unknown action name = load-time error, not silent skip
- [ ] Glob match cache: precompute the set of prototypes matching each selector

### [P1] Reentrancy guard
- [ ] Replace the per-OS thread-storage code in `src/v1/common.c` (`g_enable_tracing`) with a single atomic-builtins flag in the engine, mirroring `src/v2/real_impls.c`'s pattern
- [ ] `retrace_engine_suspend(retrace_engine_t*)` / `_resume()` for internal libc calls

### [P2] Performance
- [ ] Per-thread context cached on `pthread_key_t`; never allocate on the dispatch hot path
- [ ] Prototype lookup: open-addressing hashtable, no allocations on lookup
- [ ] Action list: contiguous array per rule (cache-friendly)

## Acceptance criteria

- `grep -rn '#include.*json' src/core/` returns no results. JSON parsing is exclusively in `src/config/json/` (TODO 04).
- `grep -rn 'JSON_' src/core/` returns no results.
- Adding a new action `foo` requires: 1 new file in `src/core/actions/foo.c` + 1 line in `src/core/actions/CMakeLists.txt`. Zero changes to engine.c.
- Adding a new prototype `bar` requires: 1 entry in the relevant `src/core/prototypes/<header>.c` + 1 line in `src/v2/funcs_symbols.S` (or its successor). Zero changes to engine.c.
- Unit tests cover every public engine function (TODO 08).

## Open questions

- Do we keep `retrace_func_define_prototypes` macro naming or rename to
  `RETRACE_PROTOTYPES_DEFINE`? Lean toward the latter for namespacing.
- Should the prototype registry be per-engine (instance-owned) or global
  (process-singleton)? Lean toward global (linker-section-based) since the
  prototypes describe libc, which doesn't change per engine instance. A
  per-engine *view* (subset) can layer on top.
