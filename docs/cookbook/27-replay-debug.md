# 27 — Time-travel replay for a trace

## Problem

You captured a 10K-line trace and you're grep'ing through JSON to
reconstruct what the binary did. The function names and arguments
are there, but the control flow is hard to see — you can't easily
"step" forward and backward, can't rewind to a specific call, can't
search forward to the next call that matches a pattern.

`retrace-replay` is an interactive TUI for traces: read the trace,
then step forward and backward, jump to any index, list nearby
calls, and search forward by regex. Like `gdb` for libc-call traces.

## Config

The replay tool works with any retrace trace. Use the default
wildcard config:

`replay-default.json`:

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

## Invocation

### 1. Capture the trace

```sh
$ retrace run --config cookbook/replay-default.json \
    --log /tmp/trace.json -- ./your-binary args...
```

### 2. Open it in replay

```sh
$ retrace-replay /tmp/trace.json
[retrace-replay] loaded 4823 events from /tmp/trace.json
> l
  #0    func=opendir     args={ name: "." }
  #1    func=readdir     args={ dirp: 0x7f... }
  #2    func=readdir     args={ dirp: 0x7f... }
  #3    func=closedir    args={ dirp: 0x7f... }
  #4    func=malloc      args={ size: 4096 }
  ...
> n
#4  func=malloc   args={ size: 4096 }
     retval=0x7f8a40000000   duration=2us
> f open
#7  func=open     args={ path: "/etc/passwd", flags: 0 }
     retval=3   duration=15us
> 1247
#1247  func=system   args={ command: "curl http://..." }
       retval=0   duration=1234567us
> p
#1246  func=getenv   args={ name: "PATH" }
       retval="/usr/local/bin:/usr/bin:..."
> q
```

### 3. Pipe to a script for batch navigation

`retrace-replay` is interactive, but you can drive it from stdin:

```sh
$ printf 'f open\nn\nn\nq\n' | retrace-replay /tmp/trace.json
```

Use this to extract a single event for a downstream script.

## Expected output

Each event display includes:

- Event index (`#N`).
- `func` name and parsed args (JSON object).
- Return value (raw pointer/int) and `call_duration_us`.

The prompt accepts the commands in the next section.

## Variations

### Command reference

| Key | Effect |
|-----|--------|
| `n` or Enter | Next event forward |
| `p` | Previous event (rewind) |
| `N` | Next 10 events |
| `<number>` | Jump to event index N |
| `l` | List next 10 events |
| `f <regex>` | Forward to next event whose func or args match |
| `q` | Quit |

### Surface captured-memory entries

If your trace includes `capture_buffer:` entries (recipe 22's
post-call memory observation), they show up as `captured:` lines
under their parent call:

```
#7  func=recv    args={ fd: 3, len: 4096 }
     captured: 47 45 54 20 2f 20 48 54 54 50 ... ("GET / HTTP...")
     retval=1024   duration=423us
```

### Combine with audit findings

Run `retrace-audit` first to get the entry indices of any
violations, then jump straight to those events in replay:

```sh
$ retrace-audit --policy share/policies/baseline.json \
    --trace /tmp/trace.json --format default \
    | jq '.findings[].entry_index'
1247
3081
4223
$ retrace-replay /tmp/trace.json
> 1247
#1247  func=system   ...
```

## Caveats

- The full log is read into memory. Sufficient for typical retrace
  runs (<1M events). For larger, swap to `tail -n +N | jq` first.
- Replay reads the file once at start; it doesn't watch for new
  entries. For live streaming, see recipe 28 (WebSocket streamer).
- The regex search is forward-only. To search backward, jump to a
  later index first and walk back with `p`.

## See also

- Recipe 22 — Decode HTTP/DNS (makes captured buffers readable).
- Recipe 24 — Audit a trace (find the entry indices worth jumping to).
- Recipe 28 — Live streaming (when you can't wait for the trace to finish).
