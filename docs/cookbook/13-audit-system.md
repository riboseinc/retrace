# 13 — Audit system() for injection

## Problem

You're reviewing a setuid binary (or any binary handling untrusted
input) and want to flag every `system()` call. `system()` with a
relative command is a classic PATH-injection vector.

## Config

`audit-system.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "system",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

`log_params` records the command string; `call_real` lets the call
proceed so the program behaves normally.

## Invocation

```sh
$ cat vulnerable.c
#include <stdlib.h>
int main(void) {
    system("id");           /* unsafe: relative path */
    system("/usr/bin/id");  /* safe: absolute path */
    return 0;
}
$ cc vulnerable.c -o vulnerable

$ retrace run --config docs/cookbook/audit-system.json -- ./vulnerable
[
  { "func": "system", "args": { "command": "id" }, "ret": 0 },
  uid=501(you) gid=20(staff) ...
  { "func": "system", "args": { "command": "/usr/bin/id" }, "ret": 0 },
  uid=501(you) gid=20(staff) ...
]
```

The first call has `"command": "id"` — a relative path. An attacker
who controls `PATH` (or the current directory) can substitute a
malicious `id` binary. The second call uses `/usr/bin/id` and is
safe.

## Variations

### Block system() entirely

Replace `call_real` with `modify_return_value_int` to refuse
execution:

```json
{
  "actions": [
    { "action_name": "log_params" },
    { "action_name": "modify_return_value_int",
      "action_params": { "retval_int": -1 } }
  ]
}
```

Now every `system()` call logs the attempt and returns -1 without
running anything.

### Audit execve too

`system()` is a wrapper around `fork`/`execve`. To catch all command
executions, including direct `execve` calls:

```json
{
  "intercept_scripts": [
    { "func_name": "system", "actions": [ ... ] },
    { "func_name": "execve", "actions": [ ... ] },
    { "func_name": "execl",  "actions": [ ... ] },
    { "func_name": "execlp", "actions": [ ... ] },
    { "func_name": "execvp", "actions": [ ... ] }
  ]
}
```

### Use the unsafe-system example

The repo ships a worked version:

```sh
$ retrace run --config examples/unsafe-system/retrace.conf.json -- ./system
```

## How it works

`log_params` looks up the prototype for `system` in
`src/core/prototypes/stdlib.c`, walks the parameters (just one:
`command`, typed as a string), and emits them as JSON. The caller's
view of the call is unchanged because `call_real` invokes real libc.

## See also

- [14 — Trace getenv() reads](14-trace-getenv.md)
- [15 — Capture network traffic](15-capture-network.md)
