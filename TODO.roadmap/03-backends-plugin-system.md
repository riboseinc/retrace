# 03 — Backend plugin system

**Status**: [ ] pending
**Layer**: 3 (interposition backends — how retrace actually intercepts calls)
**Depends on**: 02
**Blocks**: 05, 06, 09

## Goal

Define a single backend interface and split the current interposition code
(Linux `LD_PRELOAD`, macOS `DYLD_INSERT_LIBRARIES`, BSD `RTLD_NEXT`, plus the
per-arch x86-64 assembly trampoline) into independent, MECE-partitioned
backend plugins. Future backends — `ptrace` (for static binaries), `eBPF`
(for kernel-visible events), Windows DLL injection, Frida-style — drop in
without touching the engine.

## Why

Today, "how retrace gets installed" and "how retrace intercepts" are fused:

- v1 hard-codes `LD_PRELOAD`/`DYLD_INSERT_LIBRARIES` via `setenv` in
  `src/v1/retrace.c::fork_cmd` and uses per-OS macros (`RETRACE_REPLACE`,
  `DYLD_INTERPOSE`) in every wrapper file.
- v2 has a single assembly trampoline in `src/v2/arch/x86-64/{linux,osx,bsd}/`
  with no way to add a non-assembly interception strategy.
- The engine (`src/v2/engine.c`) takes a `void *arch_spec_ctx` from the
  trampoline, so the "arch" is already leaking into the engine.

This blocks Windows support (no `LD_PRELOAD`), blocks ptrace-based tracing of
statically-linked binaries, and blocks any future kernel-assisted approach.

## Architecture

```c
/* include/retrace/backend.h */

typedef enum {
    RETRACE_BACKEND_RANK_PREFERRED = 0,  /* use if available */
    RETRACE_BACKEND_RANK_FALLBACK  = 1,  /* use if no preferred works */
} retrace_backend_rank_t;

typedef struct retrace_backend {
    const char *name;                  /* "preload-elf", "preload-macho", "ptrace", ... */
    const char *description;
    retrace_backend_rank_t rank;

    /* Probe whether this backend can trace a given target on this host.
     * Returns 1 if it can, 0 if not, -1 on error. */
    int (*probe)(retrace_engine_t *eng, const char *target_path);

    /* Install retrace into a not-yet-started target process.
     * Spawns the target with the backend's mechanism already active.
     * Returns the child PID or -1 on error. */
    pid_t (*spawn)(retrace_engine_t *eng,
                   const char *target_path,
                   char *const argv[],
                   char *const envp[]);

    /* Attach to an already-running target. Optional — not all backends
     * support this (LD_PRELOAD does not, ptrace does).
     * Returns 0 on success, -1 on error or unsupported. */
    int (*attach)(retrace_engine_t *eng, pid_t target_pid);

    /* Detach / uninstall. */
    int (*detach)(retrace_engine_t *eng);

    /* Per-call hook called by the trampoline. Backend translates its
     * native frame representation into retrace_thread_context. */
    void (*translate_frame)(retrace_thread_context_t *ctx, void *native_frame);
} retrace_backend_t;

/* Registration — backends call this from their constructor. */
int retrace_backend_register(const retrace_backend_t *backend);

/* Engine API */
const retrace_backend_t *retrace_backend_select(retrace_engine_t *eng,
                                                 const char *target_path,
                                                 const char *requested_name);
```

Each backend lives in its own subdirectory and ships as a separate CMake
target. The CLI links whichever backends it needs; downstream library users
can opt out of any they don't want.

## Backend inventory

| Name | Status | Layer mechanism | Static binaries | Windows |
|------|--------|-----------------|-----------------|---------|
| `preload-elf` | migrate from v2 | `LD_PRELOAD` + per-arch trampoline | no | no |
| `preload-macho` | migrate from v2 | `DYLD_INSERT_LIBRARIES` + `__interpose` | no | n/a |
| `preload-bsd` | migrate from v2 | `RTLD_NEXT` + per-arch trampoline | no | n/a |
| `preload-mingw` | new | MinGW DLL injection via `SetWindowsHookEx` / Detours | n/a | yes (MSYS) |
| `preload-msvc` | new | MSVC DLL injection | n/a | yes (native) |
| `ptrace` | future | `PTRACE_SEIZE` + `PTRACE_INTERRUPT` | yes | no |
| `frida` | future | Frida native bridge | yes | yes |
| `ebpf` | future | `uprobe` / `bpf_trampoline` | yes | no |

The first three are extracted from current v2 code; the rest are placeholders
for future work.

## File layout

```
src/backends/
├── interface.h               /* the retrace_backend_t definition (above) */
├── registry.c                /* retrace_backend_register/select */
├── preload_elf/
│   ├── backend.c             /* spawn() uses posix_spawn + LD_PRELOAD */
│   ├── trampoline.S          /* was: src/v2/arch/x86-64/linux/arch_spec_top.S */
│   ├── trampoline.c          /* was: src/v2/arch/x86-64/linux/arch_spec_bottom.c */
│   └── CMakeLists.txt        /* builds backend-preload-elf target */
├── preload_macho/
│   ├── backend.c             /* spawn() uses posix_spawn + DYLD_INSERT_LIBRARIES */
│   ├── interpose.S           /* was: src/v2/arch/x86-64/osx/arch_spec_top.S */
│   ├── interpose.c
│   └── CMakeLists.txt
├── preload_bsd/
│   ├── backend.c             /* RTLD_NEXT-based */
│   ├── trampoline.S
│   ├── trampoline.c
│   └── CMakeLists.txt
├── preload_mingw/
│   ├── backend.c             /* new — MinGW DLL injection */
│   └── CMakeLists.txt
├── ptrace/                   /* future */
└── ebpf/                     /* future */
```

## Migration path from current v2

The current `src/v2/arch/x86-64/{linux,osx,bsd}/arch_spec_top.S` files are
literally the per-backend trampolines, just mis-located. The migration is
mechanical:

1. Move `src/v2/arch/x86-64/linux/arch_spec_{top.S,bottom.c,macros.h}` to
   `src/backends/preload_elf/{trampoline.S,trampoline.c,macros.h}`.
2. Replace `setenv("LD_PRELOAD", ...)` in `src/v2/retrace_v2.c::fork_cmd` with
   a call to `backend->spawn(...)`.
3. Add a `retrace_backend_register(&preload_elf_backend)` in
   `src/backends/preload_elf/backend.c`'s constructor.
4. The engine still calls `retrace_engine_dispatch(func_name, frame)` — the
   `translate_frame` callback translates between the backend's frame layout
   and `retrace_thread_context`.

## Tasks

### [P0] Interface
- [ ] Define `retrace_backend_t` in `include/retrace/backend.h`
- [ ] Define `retrace_backend_register` / `retrace_backend_select` in `src/backends/registry.c`
- [ ] Constructor-section scan pattern (mirror prototype/action registries)

### [P0] Extract preload-elf
- [ ] Move Linux assembly trampoline + frame layout
- [ ] Move `LD_PRELOAD` env-setting code from `fork_cmd` into `spawn()`
- [ ] Use `posix_spawn` instead of `fork + execv` (faster, no address-space duplication)
- [ ] Backend tests (TODO 08): every prototype in `src/v2/funcs_symbols.S` is exercised

### [P0] Extract preload-macho
- [ ] Move macOS `__DATA,__interpose` section setup
- [ ] Move `DYLD_INSERT_LIBRARIES` + `DYLD_FORCE_FLAT_NAMESPACE` env-setting
- [ ] macOS SIP detection: warn (don't fail) when target is in a SIP-protected path

### [P1] Extract preload-bsd
- [ ] Move FreeBSD/OpenBSD/NetBSD `RTLD_NEXT`-based symbol resolution
- [ ] Verify OpenBSD `STAILQ_*` → `SIMPLEQ_*` rewrite still needed (likely vestigial)

### [P1] Backend selection logic
- [ ] Default: probe each registered backend; pick highest-rank one that returns 1 from `probe()`
- [ ] CLI `--backend=name` overrides
- [ ] `retrace backends list` subcommand prints all registered backends

### [P2] Future backends (do not implement in Phase 2 — placeholder only)
- [ ] `preload-mingw` skeleton
- [ ] `ptrace` skeleton (Linux only, for static binaries)
- [ ] `frida` skeleton (optional dynamic plugin loaded via `dlopen`)

## Acceptance criteria

- `grep -rn 'LD_PRELOAD\|DYLD_INSERT' src/` returns results only in
  `src/backends/preload_elf/` and `src/backends/preload_macho/`.
- `grep -rn 'arch_spec' src/core/` returns nothing.
- Adding `ptrace` backend = new directory `src/backends/ptrace/` + 1 line in
  top-level `CMakeLists.txt`. Engine unchanged. CLI unchanged.
- Every backend has an integration test that runs a target binary end-to-end
  and verifies the `log_params` action fires.

## Open questions

- Should backends be runtime-loadable plugins (`dlopen`) or compile-time
  linked? Lean toward compile-time by default for predictability; the engine
  can support `dlopen` plugins via the same registry interface later if needed.
- For Windows MSVC builds: do we use Microsoft Detours (commercial) or write
  our own trampoline? Lean toward a from-scratch trampoline to avoid licensing.
