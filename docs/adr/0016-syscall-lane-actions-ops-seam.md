# ADR-0016: Syscall-lane actions — the arch-spec seam made polymorphic

- Status: Accepted
- Date: 2026-09-12
- Deciders: retrace maintainers

## Context

The ptrace backend (static Linux binaries) observes syscalls:
`PTRACE_SYSCALL` stops at every syscall boundary, the loop
translates registers into a portable `retrace_ptrace_frame`, and
hands it to `retrace_engine_wrapper()` — the same entry the
preload trampolines use. The frame even carries the fields a
denial needs (`skip_real`, `forced_retval`, `arg_modified[]`),
and the loop honors them.

But nothing ever sets them. The `retrace_as_*` operations the
engine drives (schedule the real call, cancel it, set the return
value, extract params) are defined once per preload backend and
cast their `arch_spec_ctx` to `WrapperSystemVFrame` — the
trampoline's stack frame. On the ptrace lane the same symbols
run against a `retrace_ptrace_frame`: `sched_real` scribbles
over `frame.arch`/`syscall_name`, `setup_params` reads register
fields that are not there (adjacent-stack garbage), and an
explicit `call_real` action would execute the function **in the
tracer**, a side effect in the wrong process. The card-06
symptom — "observes but cannot deny" — is one facet of a
monomorphic seam hosting a second frame type.

The seam exists and is the right one: actions (`log_params`,
`sandbox`, `modify_return_value_int`, ...) are already
frame-agnostic — they operate on parsed `params[]` and write
back through `retrace_as_*`. Only the implementation beneath
those functions assumes the preload frame.

## Decision

1. **The `retrace_as_*` functions become a polymorphic ops
   table.** A new `struct retrace_as_ops` (sched_real,
   cancel_sched_real, set_ret_val, setup_params, call_real) is
   declared alongside the existing declarations. Each backend
   supplies a `const` ops instance; the per-backend definitions
   rename to trampoline-qualified statics
   (`retrace_as_trampoline_sched_real`, ...) and publish
   `retrace_as_ops_default`. A shared dispatcher (`as_ops.c`)
   defines the public `retrace_as_*` symbols: the verbs take the
   calling `ThreadContext` as their leading argument and select
   the table from `ctx->lane_ops`, falling back to the
   per-platform default when unset. Cost: one context-field
   check and an indirect call on a path measured in
   microseconds — noise (the F2 lesson: 22 ns dispatch tail).

   Deliberately NOT thread-local storage: threads spawned
   mid-boot (the logger flusher) carry broken TLV under
   DYLD_INSERT on macOS, and any `_Thread_local` in the engine
   path aborts with `_tlv_bootstrap`. The ThreadContext is
   already per-thread and boot-safe.

2. **Actions execute at the syscall-entry stop by register
   rewrite.** The ptrace trace loop swaps in the ptrace ops
   around its `retrace_engine_wrapper()` call. Denial/skip =
   the existing mechanism: rewrite the return register and
   (x86_64) `orig_rax = -1`, continue with `PTRACE_SYSCALL` —
   the kernel jumps straight to syscall-exit. No seccomp filter
   is installed in the tracee: seccomp/landlock compilation of
   a declared set is the `enforce` lane's tool and must run
   before exec; this is live, per-call policy on the observation
   lane, decided by the same JSON scripts. MECE: one lane
   compiles declared-sets into the target, the other applies
   scripts at each boundary.

3. **On the syscall lane, the kernel IS the real
   implementation.** The ptrace ops map the engine's lifecycle
   verbs honestly:
   - `sched_real` → no-op: continuing the tracee already runs
     the real syscall.
   - `cancel_sched_real` → `skip_real = 1`, `forced_retval =
     -ENOSYS` (the engine cancels by default when a script
     matches; a script that never allows denies).
   - `call_real` (the action's in-process invocation) → clear
     the skip: the action's intent is "let the real call run",
     which on this lane the kernel does. The ops return a
     DEFERRED sentinel (2^52+1, exact as the double the JSON
     actions carry) so the engine tail's unconditional
     ret-val write cannot masquerade as a result the kernel
     never produced.
   - `set_ret_val(v)` → if the stop is still denied, set
     `forced_retval = v`; if allowed, v is recorded and applied
     at the syscall-**exit** stop (the loop already stops
     there), giving `modify_return_value_int` after `call_real`
     its exact preload semantics. The sentinel is dropped on
     the floor (no override); a script that literally modifies
     to 2^52+1 loses that override -- pathological config,
     kernel result stands.
   - `setup_params` → params come from `frame->arg_in[0..5]`;
     string-typed pointer params (`ref_type_name "sz"`) are
     **materialized** from the tracee's address space via
     `process_vm_readv` (the frame carries the tracee pid) into
     tracer-owned buffers with `free_val` set. Actions then
     read paths exactly as they do on the preload lane; nothing
     dereferences tracee pointers in the tracer. Variadic
     protos are refused (the engine skips actions; the kernel
     runs the call) -- syscalls have no varargs.

4. **Deny maps to the kernel's errno convention.** `sandbox`'s
   deny sets `errno = EACCES` and `ret_val = -1` (the libc
   convention), and records the paired errno on the context
   (`ret_errno`) — by the time the engine tail runs, the deny's
   own logging has clobbered the live errno (CI-observed:
   ENOSYS surfacing where EACCES was denied). The ptrace
   `set_ret_val` translates `-1` to `-ret_errno`, falling back
   to the live errno and then to `-EPERM` — the raw-syscall
   return convention a static tracee's libc wrapper understands.
   The recording is lane-agnostic (the preload lanes ignore it;
   their same-process errno already reaches the caller); only
   the translation lives in the ops implementation.

5. **Syscall-class ↔ action-name mapping is the existing
   syscall table.** Actions key on canonical names ("open",
   "openat"); `syscall_table.c` maps numbers to those names. A
   syscall without a table entry is untraceable by name and
   simply passes through — adding syscalls remains a table
   append, engine and actions unchanged.

## Out of scope (documented, not silently dropped)

- **String-arg rewrite** (`modify_in_param_str`, sandbox's
  `decoy_dir` deception): rewriting a path at the syscall stop
  needs scratch pages in the tracee's address space to hold the
  new string. Parked until a lane needs it; the action applies
  only its deny branch here.
- **In-param write-back of int args**: `arg_modified[]` write-
  back of modified non-pointer params is a small follow-up once
  a script asks for it; no action in the current set requires
  it on this lane.
- **Non-string ref materialization**: struct/array refs surface
  as NULL (the log's deref-skip path) rather than being copied
  -- the deref layer's size model reads in-process memory and
  cannot size tracee structs yet. The absence is visible in
  the log, never garbage.
- **Variadic dispatch**: syscall args are never variadic; the
  ops table has no variadic member.

## Nested dispatch and the lane override

A linked-in retrace library interposes the TRACER's own libc
(the library's globals win symbol resolution), so logging
inside an active ptrace dispatch re-enters the engine with a
trampoline frame -- while the thread-local lane override is
still installed. Those nested entries would run through the
ptrace ops against trampoline frames: silent frame corruption
and, because each nested no-script exit clears the reentrance
guard, unbounded recursion (the same signature as the historic
macOS lldb stack overflow).

The dispatcher therefore honors a lane override only when the
dispatch depth on the thread's context is <= 1 (0 = a direct
call — the unit-test contract; 1 = the lane's own frame, always
the outermost entry on its thread). Depth >= 2 is a nested
entry: trampoline lane, default ops. The depth counter and the
lane pointer both live on the ThreadContext — per-thread state
without TLS (see Decision 1's boot-safety note).

## Trace-loop gating

Syscalls without a libc prototype (`exit_group`,
`rt_sigaction`, ...) never reach the engine: the loop checks
`retrace_proto_cached()` first and passes them through. The
engine's no-real-impl bail path would otherwise deny them
(returning -EPERM to `exit_group` wedges the tracee forever);
unprototyped syscalls have no script by definition, so the
skip is exact, and it also keeps the per-syscall cost off the
name-lookup path for the hot untraceable set.

## Consequences

- The engine, all actions, config parsing, and the logger are
  untouched — the deep modules stay deep; the new behavior is
  one more ops implementation (the open/closed addition this
  seam was always meant to allow; "one adapter = hypothetical
  seam, two = real" — the ptrace adapter makes it real).
- The preload lanes compile to the same machine behavior plus a
  TLS-load + indirect call per engine entry.
- The ptrace lane gains honest action semantics: log with real
  strings read from the tracee, deny with `-errno`, return-value
  rewrite at exit, all from the same JSON scripts the preload
  lanes use.
- Two ops instances now live in one library (default + ptrace);
  the thread-local current keeps a traced tracer's own
  intercepts on their own ops if a preload lane and the trace
  loop ever share a process.

## Alternatives considered

- **A separate engine entry for the ptrace lane** (`retrace_
  engine_wrapper_ptrace`) — duplicates script lookup and action
  iteration; the lanes drift. Rejected on DRY grounds.
- **An ops pointer at offset 0 of every frame** — requires
  touching every asm trampoline on every backend for zero
  additional type safety over the TLS current. Rejected.
- **Seccomp filter install at exec** for static-lane denial —
  policy freezes at exec (no live changes), no per-call logging,
  and it belongs to the `enforce` lane's declared-set model.
  Kept out of this lane on MECE grounds (see Decision 2).
- **In-process `call_real` in the tracer** (status quo risk) —
  executes side effects in the wrong process; categorically
  wrong for a security tool.
