# 03 — Count calls per function

## Problem

You want a quick histogram: which libc functions does this binary call
the most? You don't need per-call timing or arguments — just the
count, so you can spot a chatty function or a tight loop.

## Config

`count-calls.json`:

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

Identical to [01-trace-all-calls](01-trace-all-calls.md). The
histogram is built from the log, not from a special action.

## Invocation

```sh
$ retrace run --config docs/cookbook/count-calls.json \
    --log /tmp/counts.json -- ./your-program

$ retrace pp /tmp/counts.json
```

`retrace pp` produces a per-function table:

```
malloc          412 calls
free            408 calls
memcpy          287 calls
memset          245 calls
strlen          189 calls
write            64 calls
read             42 calls
open              8 calls
…
```

## Variations

### Top 10 by call count

Pipe through `head`:

```sh
$ retrace pp /tmp/counts.json | head -10
```

### Only I/O calls

Filter the log to just file/network functions before pretty-printing.
Set `RETRACE_LOGGER_ALLOWED_FUNCS`:

```sh
$ RETRACE_LOGGER_ALLOWED_FUNCS=open,openat,read,write,close,connect,send,recv \
    retrace run --config docs/cookbook/count-calls.json \
    --log /tmp/io-only.json -- ./your-program
$ retrace pp /tmp/io-only.json
```

### Compare two binaries

Run the same config against two binaries, then diff the pretty-printed
output:

```sh
$ retrace run --config docs/cookbook/count-calls.json \
    --log /tmp/v1.json -- ./binary-v1
$ retrace run --config docs/cookbook/count-calls.json \
    --log /tmp/v2.json -- ./binary-v2
$ diff <(retrace pp /tmp/v1.json) <(retrace pp /tmp/v2.json)
```

Functions that moved sharply up or down between versions are usually
worth investigating.

## How it works

`log_params` emits one JSON object per call. The CLI's built-in
`pp` subcommand reads the JSON-lines log and aggregates call counts
per `func` field in pure C — no Python, no `jq`, no extra tools.

For richer aggregation (percentages, sort by total time, filter by
duration), see [04-time-each-call](04-time-each-call.md).

## See also

- [01 — Trace every libc call](01-trace-all-calls.md)
- [04 — Time each call](04-time-each-call.md)
