# 11 — Deterministic fuzzing seed

## Problem

A fuzz run found a bug. You want to reproduce it exactly — same
sequence of failures, same everything — to debug and verify the fix.

## Config

`deterministic-fuzz.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "fuzzing_seed",
      "actions": [
        { "action_name": "fuzzing_seed",
          "action_params": { "seed": 1729 }
        }
      ]
    },
    {
      "func_name": "malloc",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "memory_fuzz",
          "action_params": { "fail_rate": 0.05 }
        }
      ]
    }
  ]
}
```

The first script sets the global RNG seed once at startup. After that,
every `rand()` call (used by `memory_fuzz` and other randomized
actions) follows the same sequence.

## Invocation

```sh
$ retrace run --config docs/cookbook/deterministic-fuzz.json -- ./your-program
(crashes at iteration 47)

$ retrace run --config docs/cookbook/deterministic-fuzz.json -- ./your-program
(crashes at iteration 47 again)
```

## Variations

### Find the right seed

Loop over seeds in a shell:

```sh
$ for seed in $(seq 1 100); do
    sed "s/1729/$seed/" docs/cookbook/deterministic-fuzz.json > /tmp/run.json
    retrace run --config /tmp/run.json -- ./your-program || echo "seed $seed crashed"
  done
```

### Combine with incomplete_io

Deterministic short-reads are great for protocol parser tests:

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
      "func_name": "read",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "incomplete_io",
          "action_params": { "rate": 0.3 } }
      ]
    }
  ]
}
```

## How it works

`fuzzing_seed` calls `srand(seed)` exactly once during the first
intercepted call. Subsequent `rand()` calls in `memory_fuzz` and
`incomplete_io` draw from the seeded sequence. As long as the program
makes the same sequence of intercepted calls, the same draws happen
in the same order — exact reproduction.

## See also

- [09 — Fuzz malloc failures](09-fuzz-malloc.md)
- [10 — Partial I/O (short reads/writes)](10-incomplete-io.md)
