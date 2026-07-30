# 01 — Trace every libc call

## Problem

You want to see every libc function your program calls, in order, with
arguments and return values. This is the "I just want to know what
this binary does" baseline.

## Config

`trace-all.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "*",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

The wildcard `"*"` matches every interceptable function. Each call
gets `log_params` (writes args + return value as JSON) then
`call_real` (invokes the real libc implementation so the program
still works).

## Invocation

```sh
$ retrace run --config docs/cookbook/trace-all.json -- /bin/ls
[
  { "time": 1785400000, "func": "opendir", "args": { ... }, "ret": 0x... },
  { "time": 1785400000, "func": "readdir", "args": { ... }, "ret": 0x... },
  ...
  file1.txt  file2.txt  file3.txt
]
```

If you don't pass `--config`, this is the default behavior.

## Expected output

A JSON array printed to stdout (or to a file with `--log FILE`).
Each entry includes:

- `time` — Unix epoch seconds.
- `module` — usually `FUNCS` for the call log, `ACT` for action-specific data.
- `severity` — INFO / WARN / ERROR.
- `message` — function name, args, return value, call duration.

## Variations

### Send logs to a file instead of stdout

```sh
$ retrace run --config docs/cookbook/trace-all.json \
    --log /tmp/trace.json -- /bin/ls
(quiet program output)
$ tail -1 /tmp/trace.json
  { "func": "exit", ... }
```

### Skip noisy functions

Use the denylist env var to suppress chatty functions:

```sh
$ RETRACE_LOGGER_EXCLUDED_FUNCS=pthread_mutex_lock,pthread_mutex_unlock \
    retrace run --config docs/cookbook/trace-all.json -- /bin/ls
```

### Pretty-print the JSON

```sh
$ retrace run --config docs/cookbook/trace-all.json -- /bin/ls 2>/dev/null \
    | python3 -m json.tool | less
```

## See also

- [02 — Filter by function name](02-filter-by-function.md)
- [03 — Count calls per function](03-count-calls.md)
