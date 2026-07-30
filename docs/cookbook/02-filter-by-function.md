# 02 — Filter by function name

## Problem

Tracing every libc call produces too much output. You only care about
a handful of functions — say, `open` and `openat` — and want everything
else to pass through silently.

## Config

`trace-open-only.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "open",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    },
    {
      "func_name": "openat",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

Functions not listed in any `intercept_scripts` entry are passed
through to real libc with no logging.

## Invocation

```sh
$ retrace run --config docs/cookbook/trace-open-only.json -- /bin/cat /etc/hostname
[
  { "func": "open", "args": { "path": "/etc/hostname", ... }, "ret": 3 },
  { "func": "openat", ... },
]
myhost.example.com
```

## Variations

### Use the env var instead of editing the config

If your config already uses `"*"` (the wildcard), narrow it at runtime
with `RETRACE_LOGGER_ALLOWED_FUNCS`:

```sh
$ RETRACE_LOGGER_ALLOWED_FUNCS=open,openat,read,write \
    retrace run --config docs/cookbook/trace-all.json -- /bin/cat /etc/hostname
```

This keeps the config generic and pushes the focus into the environment.

### Exclude instead of include

The inverse — trace everything *except* `pthread_mutex_*`:

```sh
$ RETRACE_LOGGER_EXCLUDED_FUNCS=pthread_mutex_lock,pthread_mutex_unlock \
    retrace run --config docs/cookbook/trace-all.json -- /bin/ls
```

## How it works

The engine looks up the function in two places:

1. The JSON `intercept_scripts` array — if there's no entry, the call
   is silently passed to real libc.
2. The per-function log filter (`retrace_logger_func_loggable`) —
   even if a script exists, the engine skips action processing when
   the function is filtered out.

Both checks happen before any action runs, so filtering is essentially
zero-cost.

## See also

- [01 — Trace every libc call](01-trace-all-calls.md)
- [16 — Multi-function script](16-multi-script.md)
