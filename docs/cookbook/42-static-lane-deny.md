# 42 — Stop a static sample: policy on the syscall lane

## Problem

The sample is statically linked — an empty PLT, nothing to
preload. Until now the ptrace lane could only WATCH it escape
(`retrace attach <pid>`); the detonation was observation, not
containment.

## Config

The syscall lane runs the engine's ordinary JSON scripts
(ADR-0016): same `sandbox` action, same `call_real` shape.
Policy for a static detonation:

```json
{
  "intercept_scripts": [
    { "func_name": "open",
      "actions": [
        { "action_name": "sandbox",
          "action_params": { "deny_paths": ["/etc/shadow",
                                             "/etc/sudoers"] } },
        { "action_name": "call_real" } ] },
    { "func_name": "openat",
      "actions": [
        { "action_name": "sandbox",
          "action_params": { "deny_paths": ["/etc/shadow",
                                             "/etc/sudoers"] } },
        { "action_name": "call_real" } ] }
  ]
}
```

## Invocation

The tracer carries the config (the engine runs in the tracer;
the tracee only feels the syscall stops):

```sh
cc -static -o sample-static sample.c     # the evidence binary
RETRACE_JSON_CONFIG=policy.json \
RETRACE_LOGGER_DEF_ENA=1 RETRACE_LOGGER_DEF_FN=evidence.log \
./driver sample-static
```

(`driver` = any process that calls `retrace_attach_process()`
on the sample's pid — the shape `test/integration/
test_ptrace_deny.py` builds. A `retrace attach` front-end for
it lands with the CLI lane.)

## What the tracee sees

The denial follows the KERNEL's convention: the syscall returns
`-EACCES`, so the sample's libc wrapper reports
`Permission denied` — indistinguishable (to the sample) from a
file it may not read. `deny_classes: ["write"]` gives read-only
detonation the same way.

## How it works (and what doesn't)

At each syscall-entry stop the trace loop dispatches the
engine under the syscall-lane ops table: `call_real` means "let
the kernel run it", denial skips the syscall with a forced
`-errno`, and `log_params` reads paths out of the tracee's
memory (`process_vm_readv`) — the tracer never dereferences a
tracee pointer. Fault injection (`modify_return_value_int`
without `call_real`) and replay work unchanged.

Not on this lane (ADR-0016 out-of-scope): rewriting string args
(decoy mode), writing back int args, and non-string derefs
(surfaces as NULL in logs). Syscalls with no libc prototype
(`exit_group`, ...) pass the engine entirely.

## Notes

Denials are ordinary logger records — the same events the
supervisor journal and OTLP stream carry (`retrace.jail.denied`),
so a static detonation audits like any other.
