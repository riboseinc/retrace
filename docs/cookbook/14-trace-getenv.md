# 14 — Trace getenv() reads

## Problem

A binary's behavior depends on environment variables, but you don't
know which ones it consults. You want a list of every `getenv()` call
with the variable name and the value seen.

## Config

`trace-getenv.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "getenv",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

## Invocation

```sh
$ retrace run --config docs/cookbook/trace-getenv.json -- ./your-program
[
  { "func": "getenv", "args": { "name": "HOME" }, "*HOME": ["/home/user"], "ret": "0x..." },
  { "func": "getenv", "args": { "name": "PATH" }, "*PATH": ["/usr/bin:..."], "ret": "0x..." },
  { "func": "getenv", "args": { "name": "LANG" }, "*LANG": ["en_US.UTF-8"], "ret": "0x..." },
  { "func": "getenv", "args": { "name": "SECRET_TOKEN" }, "*SECRET_TOKEN": ["(null)"], "ret": "0x0" }
]
```

## Variations

### Find secrets being read

Scan for environment variables that look like credentials:

```sh
$ retrace run --config docs/cookbook/trace-getenv.json -- ./your-program 2>&1 \
    | grep -iE 'TOKEN|SECRET|PASSWORD|API_KEY'
```

### Mock an env var without exporting it

Override what `getenv()` returns for a specific variable:

```json
{
  "intercept_scripts": [
    {
      "func_name": "getenv",
      "actions": [
        { "action_name": "modify_in_param_str",
          "action_params": {
            "param_name": "name",
            "new_val": "HOME"
          }
        },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

This rewrites every `getenv("WHATEVER")` into `getenv("HOME")` —
useful for forcing the binary down a specific code path.

### Use the getenv-fuzzing example

The repo ships a worked version with memory fuzzing applied to
`getenv`'s return buffer:

```sh
$ retrace run --config examples/getenv-fuzzing/getenv-memory-fuzz.json -- ./your-program
```

## How it works

`getenv`'s prototype lives in `src/core/prototypes/stdlib.c`. The
`name` parameter is typed as a `PA_STRING`, so `log_params`
dereferences the pointer and includes the string in the JSON output.
The return value is also a string (`char *`), logged as `ret`.

## See also

- [02 — Filter by function name](02-filter-by-function.md)
- [13 — Audit system() for injection](13-audit-system.md)
