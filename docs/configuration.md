# retrace configuration reference

A retrace config is a JSON object describing one or more
*intercept scripts*. Each script binds a function name (or wildcard)
to an ordered list of *actions*. When the target binary calls that
function, retrace runs each action in sequence, optionally modifying
the call's arguments or return value before the caller sees them.

The same config format works on every platform — Linux, macOS,
Windows, FreeBSD, Android. The file is parsed with parson and tolerates
// and /* */ comments.

## Top-level shape

```json
{
  "intercept_scripts": [
    {
      "func_name": "<function-name-or-*>",
      "actions": [
        {
          "action_name": "<action>",
          "action_params": { "...": "..." }
        }
      ]
    }
  ]
}
```

| Field                | Type     | Required | Notes                                            |
|----------------------|----------|----------|--------------------------------------------------|
| `intercept_scripts`  | array    | yes      | One entry per function (or wildcard).            |
| `intercept_scripts[].func_name` | string | yes | Function name, or `"*"` to match every interceptable function. |
| `intercept_scripts[].actions`  | array  | yes | Ordered list. Each action runs in sequence.      |

The first matching script wins. If multiple scripts match the same
function, only the first one in the array runs.

## Action shape

```json
{
  "action_name": "modify_in_param_str",
  "action_params": {
    "param_name": "path",
    "new_str": "/tmp/fake"
  }
}
```

| Field           | Type   | Required | Notes                                       |
|-----------------|--------|----------|---------------------------------------------|
| `action_name`   | string | yes      | One of the registered action names.        |
| `action_params` | object | depends  | Required by most actions; see reference.   |

Action names are stable across releases. New actions may be added; no
action is renamed or removed within a major version (see
[ADR-0006](adr/0006-semantic-versioning.md)).

## Built-in actions

Actions are grouped by the verb they implement. See also the
[cookbook action reference](cookbook/README.md#action-reference) for
the compact table form.

### Observe

Actions that do not affect the call's outcome.

#### `log_params`

Emits the call's arguments and return value as a JSON line to the log
output. The most common action — usually paired with `call_real` so
the program still works.

```json
{ "action_name": "log_params" }
```

Optional `action_params`:

| Param          | Type    | Effect                                              |
|----------------|---------|-----------------------------------------------------|
| `omit_params`  | array   | Parameter names to omit from the log (e.g. large buffers). |

#### `call_real`

Invokes the real libc implementation. Without this, the caller
receives whatever return value (if any) the script synthesized.

```json
{ "action_name": "call_real" }
```

No `action_params`.

#### `fuzzing_seed`

Pins the global RNG to a known value so `memory_fuzz` and
`incomplete_io` produce reproducible failure patterns. Apply once at
the top of the first script you want to seed.

```json
{ "action_name": "fuzzing_seed", "action_params": { "seed": 1498729252 } }
```

| Param  | Type   | Required | Notes                                       |
|--------|--------|----------|---------------------------------------------|
| `seed` | number | yes      | Any unsigned int. Zero is a valid seed.     |

Without this action, the seed defaults to `getpid()`, so each run
gets different failures.

#### `capture_buffer`

Reads N bytes from a pointer parameter **after** the real call
returns and logs the contents (post-call memory observation). Use
this to capture what `recv` filled, what `read` loaded, what
`snprintf` wrote &mdash; not just the return value.

```json
{ "action_name": "capture_buffer",
  "action_params": {
    "param_name": "buf",
    "size_param": "len",
    "format": "hex"
  } }
```

| Param           | Type   | Required | Notes                                                          |
|-----------------|--------|----------|----------------------------------------------------------------|
| `param_name`    | string | yes      | Name of the pointer parameter to read.                         |
| `size_param`    | string | no       | Name of the integer parameter holding the buffer length.       |
| `max_bytes`     | number | no       | Cap on bytes to read. Default 4096; hard cap 4096.             |
| `format`        | string | no       | `hex` (default) or `string`. String replaces non-printable    |
|                 |        |          | bytes with `.` (preserves `\n`/`\r`/`\t`).                     |

If `size_param` is omitted, captures `max_bytes` bytes. If
`size_param` exceeds `max_bytes`, truncates to `max_bytes`. The
action never aborts the script (always returns 0); it logs the
captured bytes as a `capture_buffer: <param>[N]=<value>` entry.

Pair with `decode_http` / `decode_dns` (recipe 22) when you want
structured fields instead of raw bytes.

### Modify

Actions that rewrite the call's arguments or return value.

#### `modify_in_param_str`

Replaces the value of a string parameter before the real call runs.

```json
{
  "action_name": "modify_in_param_str",
  "action_params": {
    "param_name": "path",
    "new_str": "/tmp/fake",
    "match_str": "/etc/passwd"
  }
}
```

| Param        | Type   | Required | Notes                                                |
|--------------|--------|----------|------------------------------------------------------|
| `param_name` | string | yes      | Must match a named parameter in the prototype.       |
| `new_str`    | string | yes      | The replacement string.                              |
| `match_str`  | string | no       | Only modify if the current value equals this.        |

The parameter must be a pointer to a NUL-terminated string in the
function's prototype.

#### `modify_in_param_int`

Replaces the value of an integer parameter.

```json
{
  "action_name": "modify_in_param_int",
  "action_params": {
    "param_name": "flags",
    "new_int": 0,
    "match_int": 1
  }
}
```

| Param        | Type   | Required | Notes                                                |
|--------------|--------|----------|------------------------------------------------------|
| `param_name` | string | yes      | Must match a named parameter in the prototype.       |
| `new_int`    | number | yes      | The replacement value.                               |
| `match_int`  | number | no       | Only modify if the current value equals this.        |

#### `modify_in_param_arr`

Replaces the contents of a byte-buffer parameter (e.g. `write`'s
`buf`, `read`'s `buf`).

```json
{
  "action_name": "modify_in_param_arr",
  "action_params": {
    "param_name": "buf",
    "new_arr": [72, 101, 108, 108, 111],
    "match_arr": [87, 111, 114, 108, 100]
  }
}
```

| Param        | Type   | Required | Notes                                                |
|--------------|--------|----------|------------------------------------------------------|
| `param_name` | string | yes      | Must match a named parameter in the prototype.       |
| `new_arr`    | array  | yes      | Byte values (0–255) to write into the buffer.        |
| `match_arr`  | array  | no       | Only modify if the current bytes equal this.         |

The buffer must be writable (i.e. the caller's own memory, not
read-only data).

#### `modify_return_value_int`

Overrides the call's return value.

```json
{
  "action_name": "modify_return_value_int",
  "action_params": { "retval_int": 0 }
}
```

| Param        | Type   | Required | Notes                                              |
|--------------|--------|----------|----------------------------------------------------|
| `retval_int` | number | yes      | The new return value (parsed as a C integer).      |

If placed *before* `call_real`, the real implementation runs but its
return value is discarded. If placed *after* `call_real`, same
effect — order does not matter for this action.

### Fault

Actions that inject failure conditions.

#### `memory_fuzz`

Randomly returns `NULL` from the wrapped allocator at a configurable
rate. Pairs with `call_real` so successful calls still work.

```json
{
  "action_name": "memory_fuzz",
  "action_params": { "fail_rate": 0.1 }
}
```

| Param       | Type   | Required | Notes                                                 |
|-------------|--------|----------|-------------------------------------------------------|
| `fail_rate` | number | yes      | Probability in [0.0, 1.0]. `0.1` = 10% of calls fail. |

When the fuzz decision is "fail", the action aborts the rest of the
script and synthesizes a `NULL` (or `0`) return value. `call_real`
must precede `memory_fuzz` for the real allocator to run on
successful calls.

#### `fuzz_str`

Replaces an incoming string parameter with a token drawn from an
AFL-style dictionary file -- content fuzzing for parsers, env
handling, and decoders without writing a harness. Deterministic
under the shared seed machinery (reproducer = config + seed +
dict file).

```json
{
  "action_name": "fuzz_str",
  "action_params": {
    "param_name": "filename",
    "dict": "/path/to/dict.txt",
    "match_str": "/etc/hosts",
    "fuzz_seed": 42
  }
}
```

| Param       | Type   | Required | Notes                                        |
|-------------|--------|----------|----------------------------------------------|
| `param_name`| string | yes      | Must be an IN `sz` pointer param.            |
| `dict`      | string | yes      | Dictionary path: one token per line; `#` lines and blanks skipped. 256 tokens max, 4096 bytes each. `@`-prefixed lines are templates: `%N%` substitutes the Nth flat token (1-9); templates only reference flat tokens (no nesting); an out-of-range reference fails the load loudly. |
| `match_str` | string | no       | Only replace when the current value matches. |
| `fuzz_seed` | number | no       | Seeds the shared RNG (once per process; also honors `RETRACE_FUZZ_SEED`). |

Place `fuzz_str` BEFORE `log_params` so the trace records the
token the callee actually received. See
`examples/fuzz-workbench` for the full flow.

#### `incomplete_io`

Truncates the return value of a read/write-like call at a
configurable rate. Useful for testing retry logic.

```json
{
  "action_name": "incomplete_io",
  "action_params": { "rate": 0.5 }
}
```

| Param  | Type   | Required | Notes                                              |
|--------|--------|----------|----------------------------------------------------|
| `rate` | number | yes      | Probability in [0.0, 1.0]. `0.5` = half of calls. |

When the fuzz decision is "truncate", the action rewrites the return
value to half of what the real call returned.

### Control

Actions that shape the run as a whole — gating, throttling, denying.

#### `delay`

Injects N milliseconds of latency before the call returns. Useful
for reproducing timing-sensitive bugs without slow networks or disks.

```json
{
  "action_name": "delay",
  "action_params": { "ms": 100 }
}
```

| Param | Type   | Required | Notes                                     |
|-------|--------|----------|-------------------------------------------|
| `ms`  | number | yes      | Milliseconds to sleep before returning.   |

Implemented via `nanosleep`. The real call has already completed by
the time this runs.

#### `call_count_limit`

Aborts the script (skipping subsequent actions) once the per-function
invocation count crosses a threshold. Pairs with
`modify_return_value_int` to simulate resource exhaustion.

```json
{
  "action_name": "call_count_limit",
  "action_params": { "limit": 5 }
}
```

| Param   | Type   | Required | Notes                                                |
|---------|--------|----------|------------------------------------------------------|
| `limit` | number | yes      | After this many calls, the action aborts the script. |

Counts are per-function-name across the process. The first
`call_count_limit` action to fire for a given function claims that
function's counter.

#### `fail_first`

Fails the first N invocations with a chosen return value, then lets
every later call through untouched — the transient-fault mirror of
`call_count_limit` (which fails *after* N). This is the retry-path
primitive: prove a library survives two `EAGAIN`s, or expose the
untested infinite retry loop.

```json
{
  "action_name": "fail_first",
  "action_params": { "fails": 2, "retval_int": -11 }
}
```

| Param        | Type   | Required | Notes                                              |
|--------------|--------|----------|----------------------------------------------------|
| `fails`      | number | yes      | How many leading invocations fail. `0` = inert.    |
| `retval_int` | number | yes      | The return value the failing calls synthesize.     |

Inside the failing window the action sets `ret_val` and aborts the
script, so `call_real` never runs; after the window the script
proceeds normally. Counts are per-function-name, first-to-fire
claims the counter — same ownership rule as `call_count_limit`,
and the two compose (transient fault on the way in, exhaustion
eventually).

#### `sandbox`

File-access policy by path. Two modes:

- `deny_paths` — allow-by-default, block the listed paths.
- `allow_paths` — deny-by-default (the jail mode, ≥ 2.12.0):
  block every path NOT on the list. Emitted by
  `retrace-profile --jail-out` (cookbook recipe 34).

```json
{
  "action_name": "sandbox",
  "action_params": {
    "allow_paths": ["/vfs/entry.dat", "/vfs/settings.dat"]
  }
}
```

A denied call returns `-1` with `errno = EACCES` before libc
executes it. When both lists are present a path must pass both
(`deny_paths` wins on conflict).

| Param         | Type    | Required            | Notes                                              |
|---------------|---------|---------------------|----------------------------------------------------|
| `deny_paths`  | array   | one of the two      | Paths to block. Prefix match when the entry ends in `/` or `\`; exact match otherwise. |
| `allow_paths` | array   | one of the two      | Paths that may be touched; everything else is blocked. Same match rules. |

The path argument is found via prototype metadata (a string
param among the first two arguments — `open(path, ...)`,
`openat(dirfd, path, ...)`); calls without a string param (e.g.
`close(fd)`) pass through unpoliced even under a `*` script.
Follow the action with `call_real` so allowed paths execute.

## Order matters

Actions run in the order they appear in the `actions` array. Some
patterns:

**Trace, then run the real call:**
```json
"actions": [
  { "action_name": "log_params" },
  { "action_name": "call_real" }
]
```

**Rewrite a path, then run:**
```json
"actions": [
  { "action_name": "modify_in_param_str",
    "action_params": { "param_name": "path", "new_str": "/tmp/fake" } },
  { "action_name": "call_real" }
]
```

**Fail after the 5th call:**
```json
"actions": [
  { "action_name": "modify_return_value_int",
    "action_params": { "retval_int": 0 } },
  { "action_name": "call_count_limit",
    "action_params": { "limit": 5 } },
  { "action_name": "call_real" }
]
```

The first 5 calls: `modify_return_value_int` sets ret to 0,
`call_count_limit` doesn't fire (under threshold), `call_real` runs
and overwrites ret with the real pointer. Effective: real pointer.

The 6th+ calls: `modify_return_value_int` sets ret to 0,
`call_count_limit` fires and aborts, `call_real` never runs.
Effective: NULL.

## Comments

parson parses with comments enabled. Both line (`//`) and block
(`/* */`) comments are allowed anywhere whitespace is allowed.

```json
{
  // First script: trace every call.
  "intercept_scripts": [
    {
      "func_name": "*",            // wildcard
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

## Default config

If no `--config` file is given (and `RETRACE_JSON_CONFIG` is unset),
retrace uses this built-in:

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

Every interceptable function is logged, then forwarded to the real
implementation.

## Validating

Run `retrace validate <config.json>` to parse and check the file
before launching the target. This catches:

- Malformed JSON.
- Unknown `action_name` values (with a "did you mean" hint).
- Missing required `action_params`.

## See also

- [CLI reference](cli.md) — every subcommand and option.
- [Cookbook](cookbook/README.md) — 20 ready-to-run recipes.
- [ADR-0007](adr/0007-json-as-canonical-config.md) — why JSON.
