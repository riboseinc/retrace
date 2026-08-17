# Architecture

How retrace's components fit together. Read this if you're
contributing to the engine, writing a new backend, or just want to
understand why the code is shaped the way it is.

Forthcoming ADRs (in `docs/adr/`) capture specific decisions; this
page is the synthesis.

## The one-paragraph model

A target binary makes a libc call. The dynamic linker (or, on
Windows, an inline hook) routes the call through retrace's
**trampoline**. The trampoline saves the call's register arguments
into a **frame** struct and invokes the **engine**
(`retrace_engine_wrapper`). The engine looks up the matching
**intercept script** in the loaded JSON config, parses the args
using the function's **prototype**, then runs the script's
**actions** in order. Each action can log, modify args, override
the return value, or inject a failure. When the script finishes,
the engine either tail-calls the **real libc function** (if
`call_real` ran) or synthesizes a return value into the frame.

## Layered view

```
┌─────────────────────────────────────────────────────────────┐
│ Target binary (your program)                                 │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ libc symbol lookup
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ Backend layer (src/backends/<name>/)                         │
│   preload_elf   preload_macho   preload_bsd                  │
│   preload_msvc  preload_mingw   ptrace                       │
│                                                               │
│   Per-arch trampoline (.S) saves regs into frame, calls      │
│   engine. Resolves real libc via dlsym/interpose/IAT.        │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ retrace_engine_wrapper(name, frame)
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ Engine layer (src/core/engine.c)                             │
│   Looks up script, parses args via prototype, runs actions.  │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ ia_<action>(t_ctx, params)
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ Action layer (src/core/actions/*.c)                          │
│   log_params  call_real  modify_in_param_*                   │
│   modify_return_value_int  memory_fuzz  incomplete_io        │
│   delay  call_count_limit  sandbox  fuzzing_seed             │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ retrace_real_impls.<fn>
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ Real libc (resolved once at init via dlsym(RTLD_NEXT, ...))  │
└─────────────────────────────────────────────────────────────┘
```

## Init order

The retrace library has a constructor that runs before any
intercepted call. The order is strict and load-bearing
(`src/core/main.c`):

```c
__attribute__((constructor(101)))   // very high priority
static void retrace_init(void)
{
    retrace_as_init();              // 1. set up backend section data
    retrace_real_impls_init();      // 2. dlsym(RTLD_NEXT, ...) all
                                    //    libc functions retrace uses
    retrace_logger_init();          // 3. log infra
    parson_alloc_hooks(...);        // 4. parson uses retrace's malloc
    retrace_conf_init();            // 5. parse RETRACE_JSON_CONFIG
    retrace_loger_update_config();  // 6. apply log config
    retrace_engine_init();          // 7. per-thread context pool
    retrace_funcs_init();           // 8. function prototype registry
    retrace_datatypes_init();       // 9. type metadata
    retrace_actions_init();         // 10. action registry
    retrace_as_init_late();         // 11. backend claims resources
}
```

Each step has a single responsibility. Out-of-order init is the
single most common source of retrace-internal bugs.

## The frame

Every backend saves register arguments into a C struct before
invoking the engine. The frame's shape depends on the calling
convention.

### Sys V x86-64 (`WrapperSystemVFrame`)

```c
struct WrapperSystemVFrame {
    /* Saved integer argument registers (per ABI). */
    long int_arg[6];                 /* rdi, rsi, rdx, rcx, r8, r9 */

    /* Saved XMM registers for FP/double args. */
    double fp_arg[8];                /* xmm0..xmm7 */

    /* Saved return address and original stack pointer. */
    long return_address;
    long original_sp;

    /* Control fields filled by the engine. */
    int   call_real_flag;
    void *real_impl;                 /* pointer to real libc fn */
    long  ret_val;                   /* synthesized return value */
};
```

### AArch64 PCS (`WrapperAArch64Frame`)

```c
struct WrapperAArch64Frame {
    /* Saved x0..x7 (integer argument registers). */
    long int_arg[8];

    /* Saved v0..v7 (FP/SIMD argument registers, 16 bytes each). */
    unsigned char fp_arg[8][16];

    /* Saved link register and original SP. */
    long link_register;              /* x30 — no return-address on stack */
    long original_sp;

    /* Control fields filled by the engine. */
    int   call_real_flag;
    void *real_impl;
    union {
        long   int_val;
        double fp_val;
    } ret_val;                       /* tagged by prototype's return type */
};
```

The frame layout is consumed by both the trampoline (which writes
into it on entry, reads return value on exit) and the engine
(which reads args, writes `call_real_flag`/`real_impl`/`ret_val`).
Changing the layout requires updating both.

## The trampoline

One trampoline per intercepted function. The trampoline's job:

1. Save registers into a stack-allocated frame.
2. Zero the control fields (`call_real_flag = 0`).
3. Load `x0`/`rdi` with the function name (a string literal).
4. Load `x1`/`rsi` with a pointer to the frame.
5. Call `retrace_engine_wrapper`.
6. After return: if `call_real_flag` is set, restore registers and
   tail-call `real_impl`. Otherwise, load `ret_val` into the
   return register and `ret`.

Per-arch assembly:

- **ELF/BSD x86-64** — `src/backends/preload_{elf,bsd}/x86_64/arch_spec_top.S`
- **Mach-O x86-64** — `src/backends/preload_macho/x86_64/arch_spec_top.S`
  (Darwin needs `.global _<fn>` prefix and interpose entries.)
- **ELF AArch64** — `src/backends/preload_elf/aarch64/arch_spec_top.S`
- **Mach-O AArch64** — `src/backends/preload_macho/aarch64/arch_spec_top.S`

The trampoline's macro `WRAPPER_ENTRY_SYSTEM_V` (or
`WRAPPER_ENTRY_AARCH64`) emits one trampoline per function. The
function inventory lives in
`src/backends/preload_*/<arch>/funcs_symbols.S`, which is now
generated from the DRY source `src/v2/funcs_symbols.def` via the
C preprocessor.

## The engine

`src/core/engine.c::retrace_engine_wrapper` is the central dispatch.

```c
int retrace_engine_wrapper(const char *func_name,
                           void *arch_spec_ctx)
{
    /* 1. Resolve real_impl for this function. */
    /* 2. If not yet initialized, return early.              */
    /* 3. Get per-thread context (ThreadContext).            */
    /* 4. Look up the prototype.                             */
    /* 5. If filter excludes func_name, return early.        */
    /* 6. Parse args from the frame into the context.        */
    /* 7. Find the intercept_script for func_name.           */
    /* 8. For each action in the script, dispatch to its     */
    /*    ia_<action>(t_ctx, action_params).                 */
    /* 9. If call_real_flag is set, signal the trampoline to */
    /*    invoke real_impl. Otherwise, set ret_val in the    */
    /*    frame.                                              */
    /* 10. Return to the trampoline.                          */
}
```

The engine itself is thin (~180 lines after the MECE refactor).
The actual work happens in the actions. Adding a new behavior =
adding a new action; the engine doesn't need to change.

## Prototypes

A prototype is a `struct FuncPrototype` describing a function's
calling convention, return type, and parameter types:

```c
struct FuncPrototype {
    enum FuncConv conv;          /* SYSCALL_V, etc.           */
    const char *type_name;       /* "int", "void *", ...      */
    struct ParamMeta params[];   /* name + type for each arg  */
};
```

Prototypes live in `src/core/prototypes/<header>.c`. Each file
defines an array of prototypes and registers it via
`retrace_func_define_prototypes(<header>)` into a linker section
the engine scans at init.

The engine uses prototypes to:

- Walk the frame's int_arg/float_arg arrays in order.
- For each param, decide whether to read from int_arg (integer
  types) or fp_arg (float types).
- Write the parsed values into the per-thread context for actions
  to consume.

If a prototype is missing, the engine can't parse args for that
function — `log_params` will emit a call record with no args.
Adding a prototype = adding a row to the appropriate
`src/core/prototypes/<header>.c` file.

## Actions

An action is a small C function registered via
`RETRACE_ACTION_REGISTER`:

```c
RETRACE_ACTION_REGISTER("memory_fuzz", ia_memory_fuzz, "...");

static int ia_memory_fuzz(struct ThreadContext *t_ctx,
                          const JSON_Object *action_params) {
    /* ... */
    return 0;       /* 0 = continue script           */
    return -1;      /* negative = abort script       */
                    /* (call_real_flag is honored)   */
}
```

Actions receive:

- `t_ctx` — per-thread context. Holds the parsed params, the
  real_impl pointer, the frame pointer, the return value slot.
- `action_params` — the JSON object from the script (may be NULL).

Actions return 0 to continue the script or a negative value to
abort. The convention is that `call_count_limit`, `memory_fuzz`
(when it decides to fail), and similar early-exit actions return
non-zero.

Action registration uses a linker section
(`__DATA,__retrace_acts` on Darwin; the ELF equivalent on Linux).
At init, `retrace_actions_init` walks the section and builds the
registry. No central action table — adding an action is purely
local to its `.c` file.

## Real-impl indirection

This is the single most important invariant in retrace:

> Every libc call inside retrace itself goes through the
> `retrace_real_impls` struct. Never call libc directly.

The `retrace_real_impls` struct (defined in
`src/core/real_impls.h`) holds function pointers for ~35 libc
symbols that retrace uses internally. At init,
`retrace_real_impls_init()` resolves them via
`dlsym(RTLD_NEXT, ...)` (or the platform equivalent).

The indirection serves two purposes:

1. **Reentrancy guard** — when retrace's engine calls e.g.
   `malloc`, the call goes to the real libc malloc, not to
   retrace's wrapper. Without this guard, every internal malloc
   would recurse back into the engine.
2. **Consistency** — retrace's internal behavior doesn't depend on
   what the target binary does to libc. If the target has
   replaced `malloc` with its own allocator, retrace's internal
   malloc still uses the real libc malloc.

Bypassing the indirection is the single most common cause of
infinite-recursion crashes. If you're writing code inside
`src/core/`, grep your code for direct libc calls and route them
through `retrace_real_impls.*`.

## Config sources

JSON configs come from one of:

1. `RETRACE_JSON_CONFIG` env var (path to a file).
2. The `--config FILE` CLI flag.
3. Built-in default: `log_params` + `call_real` for every function.

The config layer (`src/config/json/conf.c`) parses with parson
(vendored in `src/config/json/parson.{c,h}`). parson supports
`//` and `/* */` comments — useful for documenting configs.

## Logs

Every intercepted call emits one JSON object to the log stream.
The log goes to:

- stdout (if `RETRACE_LOGGER_DEF_STDOUT_ENA != 0`)
- a file (if `RETRACE_LOGGER_DEF_FN` is set, or `--log FILE`)
- both (default)

Log entries include: timestamp, function name, parsed args, return
value, `call_duration_us`.

Since v2.3.0 the hot path is a **lock-free SPSC ring** per
thread: the intercepting thread pushes an entry with two relaxed
loads and one release store (no mutex on the write side), and a
single background flusher thread drains all rings at a 1ms
cadence and performs the actual I/O. The flusher disables
interception for its own thread to avoid a feedback loop. Set
`RETRACE_LOGGER_RING=0` for synchronous writes (OHOS/QEMU/debug
builds where thread creation during init is fragile).

`retrace pp <log.json>` pretty-prints a log as text.
`retrace html <log.json>` converts it to an interactive HTML page.

## The tooling ecosystem

The standalone tools (`retrace-audit`, `retrace-diff`, ...) are
consumers of the same JSON log the library produces. Each is
decomposed into pure modules with the CLI as a thin driver —
which is what makes them unit-testable without I/O:

```
tools/audit-converter/
  policy.c     rules + matching        (policy_rule_matches)
  scan.c       apply policy to trace   (audit_scan_trace)
  format.c     render findings         (default JSON / SARIF 2.1.0)
  pdf_writer.c PDF 1.4 deliverable    (cover + summary + findings)
  audit.c      CLI: parse args, load, scan, format

tools/trace-diff/
  normalize.c  trace aggregation       (per-func counts + durations)
  threshold.c  gating math             (diff_exceeds_threshold)
  lcs.c        order alignment         (diff_lcs_len / diff_lcs_walk)
  stats.c      z-score math            (diff_stats_compute)
  diff.c       CLI: parse args, load, diff, print
```

Every module has a matching `test/unit/test_*.c` that links it
standalone. The module chains are the reason that's possible:
pure logic in small files, side effects (files, stdout)
concentrated in the CLI driver.

The output of every producer is byte-compatible with the input of
every consumer — `retrace run`, `retrace attach`, the Frida
bridge, and the eBPF bridge all emit the same JSON shape, and
audit/diff/replay/ws/to-otlp all read it. Pipelines compose
without format conversion.

## Per-platform backends

Each backend owns exactly one (OS, arch) combination. MECE by
design.

| Backend | OS | Arch | Mechanism |
|---|---|---|---|
| `preload_elf` | Linux, OHOS | x86_64, aarch64 | `LD_PRELOAD` |
| `preload_bsd` | FreeBSD, OpenBSD, NetBSD | x86_64 | `LD_PRELOAD` |
| `preload_macho` | macOS | x86_64, arm64 | `DYLD_INSERT_LIBRARIES` |
| `preload_msvc` | Windows | x86_64, arm64 | Inline hook |
| `preload_mingw` | Windows | x86_64 | Inline hook |
| `ptrace` | Linux | x86_64, aarch64 | `ptrace(2)` |

Each backend implements `retrace_backend_t` from
`include/retrace/backend.h` and self-registers via the constructor
section. `retrace_backend_select` walks the registry and picks the
highest-rank backend whose `probe()` succeeds.

The `ptrace` backend owns two niches the preload backends
structurally cannot: statically-linked binaries (their probe
checks for a missing `PT_INTERP`) and, since v2.4.0, **attaching
to an already-running process** — `retrace attach <pid>` calls the
public `retrace_attach_process()`, which selects the ptrace
backend explicitly (attach semantics bypass the probe: for a
process you cannot exec, ptrace is the sole native mechanism
regardless of how the binary was linked). Its trace loop
synthesizes a frame per syscall stop and forwards it to the same
process-global `retrace_engine_wrapper` the preload trampolines
use, so the JSON config applies unchanged.

Adding a new backend (e.g., `frida`, `ebpf`) = adding a new
directory under `src/backends/<name>/` and implementing the
callback interface. No engine change needed.

## See also

- [Development guide](development.md) — building, testing,
  contributing.
- [Configuration reference](configuration.md) — the JSON schema.
- [CLI reference](cli.md) — every subcommand.
- [ADRs](adr/README.md) — architecture decision records, including:
  - ADR-0002 layered architecture
  - ADR-0003 plugin pattern for backends
  - ADR-0004 plugin pattern for config sources
  - ADR-0007 JSON as canonical config
  - ADR-0008 opaque public types
  - ADR-0009 from-scratch Windows trampoline
  - ADR-0010 AArch64 float params from day one
  - ADR-0013 engine MECE split
