# 17 — Per-return-address routing

## Problem

You want to apply a script only when a function is called from a
specific caller. Examples:

- "Fail `open()` only when called from `authenticate_user`."
- "Mock `getuid()` only when called from `privilege_check`."
- "Log `malloc()` only when called from the parser module."

Without per-return-address routing, every call to a function runs
the same script. Targeted debugging means filtering the log by
hand afterwards.

## Config

retrace captures the return address (`ret_addr`) of each
intercepted call. The `caller_matches` array on each script lets
you gate the script on that address via three match kinds
(OR-semantics: any match wins):

- `address` — exact return address (from the log of a prior run)
- `symbol` — caller's symbol name (resolved via `dladdr`)
- `offset_in_module` — return address minus the module's load
  address (ASLR-safe)

`by-caller.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "open",
      "caller_matches": [
        { "match_type": "symbol", "value": "load_config_file" }
      ],
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    },
    {
      "func_name": "open",
      "caller_matches": [
        { "match_type": "symbol", "value": "authenticate_user" }
      ],
      "actions": [
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": -1 } }
      ]
    }
  ]
}
```

## Invocation

```sh
$ retrace run --config cookbook/17-per-return-address.json -- ./your-binary
```

## Expected output

For the config above, only `open()` calls originating from
`load_config_file` get logged, and only `open()` calls from
`authenticate_user` are forced to return `-1`. All other `open()`
calls pass through untouched (no script matches).

The JSON log shows which script fired for each call:

```json
{
  "func": "open",
  "caller": "load_config_file",
  "actions_run": ["log_params", "call_real"]
}
```

## How to discover the right match value

### symbol

Use `nm`, `objdump -t`, or `llvm-nm` on the caller binary:

```sh
$ nm your-binary | grep authenticate_user
0000000100002a40 T _authenticate_user
```

### address

Run the binary under retrace with logging first. The log includes
the return address for each call:

```sh
$ retrace run --config cookbook/01-trace-all-calls.json -- ./your-binary
$ grep '"func": "open"' retrace.log | jq .caller_address
```

Then put that exact address in the `value` field.

### offset_in_module

```sh
$ objdump -d your-binary | grep -A1 '<authenticate_user>'
```

Subtract the module's load address (printed in `/proc/<pid>/maps`
on Linux, or `vmmap` on macOS) from the call-site address.

For ASLR-safe configs that work across re-runs, prefer `symbol`
or `offset_in_module` over raw `address`.

## Variations

- **Multiple match kinds OR'd together**: any entry that matches
  wins. The first script in the array that matches takes
  precedence (same rule as today's `func_name` matching).
- **Combine with the existing single-value `return_addr` field**:
  legacy configs keep working. If both are present, `caller_matches`
  takes precedence.
- **Wildcard `func_name: "*"` with `caller_matches`**: apply a
  script to every function called from a specific caller. Useful
  for "trace everything the parser touches."

## Performance note

`symbol` and `offset_in_module` matches call `dladdr`, which is
~10µs per invocation on macOS. The default-config hot path (no
`caller_matches`) is unaffected. If your config has dozens of
`symbol` matches, consider using `address` for hot paths and
reserving `symbol` for setup/teardown phases.

## See also

- [Recipe 02: Filter by function](02-filter-by-function.md)
- [Recipe 12: Fail specific syscalls](12-fail-specific.md)
- [Recipe 16: Multi-function script](16-multi-script.md)
- TODO.complete/17-per-return-address-routing.md (design)
