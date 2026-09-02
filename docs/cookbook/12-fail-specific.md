# 12 — Fail specific syscalls

## Problem

You want to test how your program handles a specific libc failure.
Maybe you want to verify that `open()` returning `-ENOENT` is handled
cleanly, or that `connect()` returning `-ECONNREFUSED` triggers the
right retry logic. Real failures are hard to reproduce; injecting
them deterministically is easier.

## Config

`fail-open.json` — fail every `open()` with `-ENOENT`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "open",
      "actions": [
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": -2 } }
      ]
    },
    {
      "func_name": "openat",
      "actions": [
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": -2 } }
      ]
    }
  ]
}
```

`-2` is `ENOENT` on Linux. Check `errno.h` for the value on your
platform. Because the action runs *instead of* `call_real`, the real
`open` never executes — the caller immediately sees the synthesized
return value.

## Invocation

```sh
$ retrace run --config docs/cookbook/fail-open.json -- ./your-program
open(path="/etc/missing") → -2 (ENOENT)  [injected]
your-program: error: configuration file not found
```

## Variations

### Fail only after the Nth call

Allow the first few calls to succeed (so the program initializes),
then fail every subsequent call. Pair `call_count_limit` with
`modify_return_value_int` and `call_real`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "open",
      "actions": [
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": -2 } },
        { "action_name": "call_count_limit",
          "action_params": { "limit": 5 } },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

The first 5 `open` calls: `modify_return_value_int` sets ret to
`-2`, `call_count_limit` doesn't fire (under threshold),
`call_real` runs and overwrites ret with the real fd. Effective: real
fd. The 6th+ calls: `modify_return_value_int` sets ret to `-2`,
`call_count_limit` fires and aborts, `call_real` never runs.
Effective: `-ENOENT`.

### Fail the first N calls, then recover (retry paths)

The inverse direction -- a *transient* fault -- is `fail_first`:
the first N invocations return your chosen error with the real
call never made; every call after runs for real. This is the
primitive for testing retry logic deterministically:

```json
{
  "intercept_scripts": [
    {
      "func_name": "open",
      "actions": [
        { "action_name": "fail_first",
          "action_params": { "fails": 2, "retval_int": -11 } },
        { "action_name": "call_real" },
        { "action_name": "log_params" }
      ]
    }
  ]
}
```

The first two `open` calls see `EAGAIN` (-11) without touching
the filesystem; the third and onward hit the real `open`. A
correct retry loop converges on the third call; an untested one
hangs or crashes -- retrace makes either visible in a single
run. `fail_first` composes with `call_count_limit` on the same
function: transient faults on the way in, exhaustion
eventually.


### Fail network connects

To make every outbound `connect()` fail with `ECONNREFUSED`
(`-111` on Linux):

```json
{
  "intercept_scripts": [
    {
      "func_name": "connect",
      "actions": [
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": -111 } }
      ]
    }
  ]
}
```

Useful for testing whether your HTTP client retries, falls back to a
cache, or surfaces a clean error to the user.

### Fail one specific path

Combine with `modify_in_param_str`'s `match_str` semantics — but
since `modify_return_value_int` has no built-in match, use a
per-path script instead:

```json
{
  "intercept_scripts": [
    {
      "func_name": "open",
      "actions": [
        { "action_name": "modify_in_param_str",
          "action_params": {
            "param_name": "path",
            "match_str": "/etc/passwd",
            "new_str": "/etc/passwd"
          } },
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": -2 } }
      ]
    }
  ]
}
```

The `modify_in_param_str` only rewrites if the current value is
`/etc/passwd` — and rewriting it to itself is a no-op for the path,
but the fact that the action *ran* confirms the match. Then
`modify_return_value_int` injects the failure. (For a cleaner
match-then-fail pattern, write a [custom action](../README.md).)

## How it works

Without `call_real` in the action list, the engine never invokes the
real libc function. The synthesized `retval_int` is what the caller
sees. This is the simplest possible fault injection — no timing, no
randomness, no state. For probabilistic failure see
[09-fuzz-malloc](09-fuzz-malloc.md); for resource-exhaustion patterns
see [10-incomplete-io](10-incomplete-io.md).

## See also

- [09 — Fuzz malloc failures](09-fuzz-malloc.md) — probabilistic OOM.
- [10 — Partial I/O](10-incomplete-io.md) — probabilistic short reads.
- [20 — Sandbox a binary](20-sandbox.md) — deny file access by path.
