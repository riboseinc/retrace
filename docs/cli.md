# retrace CLI reference

The `retrace` binary is a thin launcher. It locates the retrace shared
library (`libretrace.so` / `.dylib` / `.dll`), sets the appropriate
preload environment variable, and `execvp`s the target. The target
then runs as if you had launched it directly — retrace's library is
already in process.

For the 90% case there are quick subcommands (`trace`, `mock`, `fuzz`,
`slow`). For the long tail there is `run`, which takes a JSON config
and unlocks the full action system.

## Quick subcommands

### `trace` — observe every call

```sh
retrace trace [--html] [funcs...] [--log FILE] [--quiet] -- <command>
```

Traces every libc call (or only the named `funcs`) by writing
`log_params` output as JSON lines. With `--html`, an interactive
self-contained HTML page is generated at `/tmp/retrace-<pid>.html`
in addition to the JSON log.

```sh
$ retrace trace malloc,free -- /bin/ls
malloc(1024) → 0x7f8e2a3b4000
free(0x7f8e2a3b4000) → 0
…

$ retrace trace --html -- /bin/ls
wrote /tmp/retrace-43892.html
```

If no `funcs` are named, every interceptable function is traced.

### `mock` — override a return value

```sh
retrace mock <func> <retval> [--log FILE] [--quiet] -- <command>
```

Forces `<func>` to return `<retval>` (parsed as a C integer — `0`,
`0x1000`, `0644`, etc.) after invoking the real implementation.

```sh
$ retrace mock getuid 0 -- ./check-root
welcome, root
```

This is sugar for `modify_return_value_int` applied to a single
function. For richer mocking (string rewriting, conditional logic,
multi-function), use `run --config`.

### `fuzz` — inject allocation failures

```sh
retrace fuzz [<func>] [--rate R] [--log FILE] [--quiet] -- <command>
```

Randomly returns `NULL` from `<func>` at failure rate `R` (0.0–1.0).
Defaults: `func=malloc`, `rate=0.05`.

```sh
$ retrace fuzz malloc --rate 0.1 -- ./server
malloc(1024) → 0x7f8e2a3b4000
malloc(2048) → NULL    # injected
malloc(512)  → 0x7f8e2a3b4200
…
```

Sugar for the `memory_fuzz` action. To fuzz `calloc`/`realloc`/
multiple allocators at once, use `run --config`.

### `slow` — inject latency

```sh
retrace slow <func> [--ms N] [--log FILE] [--quiet] -- <command>
```

Adds `N` milliseconds of latency (via `nanosleep`) before every call
to `<func>` returns. Default: `ms=100`.

```sh
$ retrace slow open --ms 100 -- ./file-heavy-app
```

Sugar for the `delay` action.

## General subcommands

### `run` — JSON config-driven

```sh
retrace run [--config FILE] [--log FILE] [--quiet] [--lib FILE] -- <command>
```

Launches the target with the given JSON config. This is the most
flexible subcommand: every action and prototype is available, and
multiple functions can be intercepted with different scripts.

```sh
$ retrace run --config docs/cookbook/20-sandbox.json -- ./untrusted-binary
```

Without `--config`, retrace falls back to the built-in default
(`log_params` + `call_real` for every function).

### `pp` — pretty-print a trace log

```sh
retrace pp <trace.json>
```

Reads a JSON-lines trace log and emits a human-readable text
summary: per-function call counts, total time, average time.

```sh
$ retrace trace malloc,free --log /tmp/trace.json -- /bin/ls >/dev/null
$ retrace pp /tmp/trace.json
malloc          412 calls   8.3 ms total   20 µs avg
free            408 calls   4.1 ms total   10 µs avg
…
```

### `html` — convert a trace log to interactive HTML

```sh
retrace html <trace.json> [-o <output.html>]
```

Reads a JSON-lines trace log and produces a self-contained
interactive HTML page: summary cards, category breakdown, filterable
call table. With `-o`, writes to the named file; without, writes to
stdout.

```sh
$ retrace trace malloc --log /tmp/trace.json -- /bin/ls >/dev/null
$ retrace html /tmp/trace.json -o /tmp/view.html
$ open /tmp/view.html
```

### `list-functions` — enumerate interceptable functions

```sh
retrace list-functions
```

Walks the function prototype registry and prints every interceptable
function name, one per line. Use to discover what retrace knows about
without reading source.

```sh
$ retrace list-functions | grep -E '^open'
open
openat
opendir
```

### `list-actions` — enumerate built-in actions

```sh
retrace list-actions
```

Prints every registered action name. Useful for validating that your
custom action loaded correctly, or for discovering what's available
without consulting docs.

```sh
$ retrace list-actions
log_params
call_real
modify_in_param_str
modify_in_param_int
modify_in_param_arr
modify_return_value_int
memory_fuzz
incomplete_io
fuzzing_seed
delay
call_count_limit
sandbox
```

### `validate` — check a JSON config

```sh
retrace validate <config.json>
```

Parses the JSON config and verifies that every `func_name` and
`action_name` is recognized. Catches typos before launch time. Exits
non-zero with a diagnostic on failure.

```sh
$ retrace validate my-config.json
ok: 4 scripts, 9 actions

$ retrace validate bad-config.json
error: unknown action 'modify_return_value' (did you mean modify_return_value_int?)
```

## Common options

These apply to `run`, `trace`, `mock`, `fuzz`, and `slow`:

| Option        | Environment var          | Effect                                       |
|---------------|--------------------------|----------------------------------------------|
| `--config F`  | `RETRACE_JSON_CONFIG`    | Path to the JSON config file.                |
| `--log F`     | `RETRACE_LOGGER_DEF_FN`  | Path to write the JSON-lines log.            |
| `--quiet`     | `RETRACE_LOGGER_DEF_ENA=0` | Suppress log output; interception still runs. |
| `--lib F`     | `RETRACE_LIB`            | Override the retrace shared library path.    |

Additional environment variables:

| Variable                       | Effect                                                 |
|--------------------------------|--------------------------------------------------------|
| `RETRACE_LOGGER_DEF_STDOUT_ENA` | `0` to suppress stdout mirroring of the log.          |
| `RETRACE_LOGGER_ALLOWED_FUNCS`  | Comma-separated allowlist of function names to log.  |
| `RETRACE_LOGGER_EXCLUDED_FUNCS` | Comma-separated denylist of function names to skip.  |

## Library resolution

The launcher finds `libretrace.so` / `.dylib` / `.dll` in this order:

1. `RETRACE_LIB` env var, if set.
2. Alongside the `retrace` binary itself (same directory).
3. Common install paths: `/usr/local/lib/`, `/usr/lib/`,
   `/usr/lib/x86_64-linux-gnu/`, `/usr/lib/aarch64-linux-gnu/`.

If none of those yield a readable library, the launcher exits with a
diagnostic. Set `RETRACE_LIB` to point at a non-standard location.

## Under the hood

Every quick subcommand writes a small JSON config to a temp file and
calls into the same code path as `run`. The configs they generate
are equivalent to the cookbook recipes — for example, `retrace mock
getuid 0` produces:

```json
{
  "intercept_scripts": [
    {
      "func_name": "getuid",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": 0 } }
      ]
    }
  ]
}
```

When a quick subcommand stops being enough — conditional logic,
multi-function scripts, custom actions — graduate to `run --config`
and the [configuration reference](configuration.md).

## See also

- [Configuration reference](configuration.md) — full JSON schema.
- [Cookbook](cookbook/README.md) — 20 recipes with copy-paste JSON.
- [Action reference](cookbook/README.md#action-reference) — table of
  built-in actions and their parameters.
