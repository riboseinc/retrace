# 09 — Fuzz malloc failures

## Problem

You want to test that your program handles out-of-memory conditions
gracefully. Real OOM is hard to trigger reliably; you'd rather inject
it deterministically.

## Config

`fuzz-malloc.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "malloc",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "memory_fuzz",
          "action_params": { "fail_rate": 0.1 } }
      ]
    },
    {
      "func_name": "calloc",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "memory_fuzz",
          "action_params": { "fail_rate": 0.1 } }
      ]
    },
    {
      "func_name": "realloc",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "memory_fuzz",
          "action_params": { "fail_rate": 0.1 } }
      ]
    }
  ]
}
```

`memory_fuzz` randomly returns `NULL` instead of the allocated
pointer, at the configured rate. `fail_rate` is `0.0` to `1.0`.

## Invocation

```sh
$ retrace run --config docs/cookbook/fuzz-malloc.json -- ./your-program
```

## Variations

### Make it deterministic

Pin the RNG seed so a failing run can be reproduced exactly:

```json
{
  "intercept_scripts": [
    {
      "func_name": "*",
      "actions": [
        { "action_name": "fuzzing_seed",
          "action_params": { "seed": 42 } }
      ]
    },
    {
      "func_name": "malloc",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "memory_fuzz",
          "action_params": { "fail_rate": 0.1 } }
      ]
    }
  ]
}
```

The seed is set once at startup. Re-running with the same seed and
the same program produces the same sequence of failures.

### Tune the failure rate

| `fail_rate` | Effect |
|-------------|--------|
| `0.0` | Never fail (disables the action). |
| `0.01` | One failure per ~100 calls. Good for long-running stress. |
| `0.1` | One failure per ~10 calls. Quick smoke test. |
| `0.5` | 50/50. Useful for testing retry logic. |
| `1.0` | Always fail. Find the first allocation the program can't live without. |

### Combine with `log_params`

Add `log_params` before `call_real` to see exactly which allocation
failed:

```json
{
  "actions": [
    { "action_name": "log_params" },
    { "action_name": "call_real" },
    { "action_name": "memory_fuzz",
      "action_params": { "fail_rate": 0.1 } }
  ]
}
```

## How it works

`memory_fuzz` calls `rand()` (seeded by `fuzzing_seed` if present) and
returns `NULL` instead of the real pointer when the draw falls below
`fail_rate`. The real libc allocator is unaffected — we just hide its
result from the caller.

## See also

- [10 — Partial I/O (short reads/writes)](10-incomplete-io.md)
- [11 — Deterministic fuzzing seed](11-deterministic-fuzz.md)
