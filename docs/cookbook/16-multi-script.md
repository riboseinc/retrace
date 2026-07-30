# 16 — Multi-function script

## Problem

You want one config that does different things to different functions:
log some, mock others, fuzz allocations, all in the same run.

## Config

`multi.json`:

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
    },
    {
      "func_name": "malloc",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" },
        { "action_name": "memory_fuzz",
          "action_params": { "fail_rate": 0.05 } }
      ]
    },
    {
      "func_name": "system",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": -1 } }
      ]
    },
    {
      "func_name": "open",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

This config:

- Makes the program think it's running as root (`getuid` → 0).
- Logs every allocation and randomly fails 5%.
- Logs and blocks every `system()` call.
- Logs every `open()`.

## Invocation

```sh
$ retrace run --config docs/cookbook/multi.json -- ./your-program
```

## Variations

### Use the wildcard as a catch-all

Add a `"*"` entry at the end to log everything else without modifying
behavior:

```json
{
  "intercept_scripts": [
    { "func_name": "getuid",  "actions": [ ... ] },
    { "func_name": "malloc",  "actions": [ ... ] },
    { "func_name": "system",  "actions": [ ... ] },
    { "func_name": "*",       "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
    ] }
  ]
}
```

Specific entries take precedence over the wildcard — retrace returns
the first match, not the last.

### Compose configs at the file level

For very large configs, split into multiple files and concatenate
with `jq`:

```sh
$ jq -s '{intercept_scripts: (map(.intercept_scripts) | add)}' \
    base.json overrides.json > composed.json
$ retrace run --config composed.json -- ./your-program
```

## How it works

`retrace_script_find` (in `src/core/script_resolver.c`) walks the
`intercept_scripts` array in order:

1. Exact `func_name` match (with optional `return_addr` filter) wins.
2. First name-only match wins if no specific match.
3. The wildcard `"*"` matches anything not yet matched.

So order matters when you have overlapping patterns. Put specific
entries first, wildcard last.

## See also

- [02 — Filter by function name](02-filter-by-function.md)
- [17 — Per-return-address routing](17-return-addr-routing.md)
