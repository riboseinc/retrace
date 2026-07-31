# 19 — CI fuzzing: catch OOM and short-IO bugs in every PR

## Problem

Your test suite passes in CI but production still hits OOM crashes
or short-read retries. The reason: CI tests almost never exercise
allocation-failure paths — `malloc` returns a real pointer every
time, `read` returns the full buffer every time.

retrace can inject those failures on every PR run, automatically,
without modifying your source. Drop one workflow file into
`.github/workflows/` and you have continuous fuzzing of your error
paths.

## Config

Save as `.github/workflows/retrace-fuzz.yml` in your repo:

```yaml
name: retrace fuzz

on:
  pull_request:
  push:
    branches: [main]

jobs:
  fuzz:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build retrace
        run: |
          git clone --depth 1 https://github.com/riboseinc/retrace.git /tmp/retrace
          cmake -B /tmp/retrace/build -S /tmp/retrace -G Ninja \
            -DCMAKE_BUILD_TYPE=Release -DRETRACE_BUILD_TESTS=OFF
          cmake --build /tmp/retrace/build

      - name: Build your project
        run: |
          # Replace with your actual build command.
          cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
          cmake --build build

      - name: Fuzz malloc (1% failure rate)
        run: |
          cat > /tmp/fuzz.json <<'EOF'
          {
            "intercept_scripts": [
              {
                "func_name": "malloc",
                "actions": [
                  { "action_name": "call_real" },
                  { "action_name": "memory_fuzz",
                    "action_params": { "fail_rate": 0.01 } }
                ]
              },
              {
                "func_name": "calloc",
                "actions": [
                  { "action_name": "call_real" },
                  { "action_name": "memory_fuzz",
                    "action_params": { "fail_rate": 0.01 } }
                ]
              }
            ]
          }
          EOF

          RETRACE_JSON_CONFIG=/tmp/fuzz.json \
          LD_PRELOAD=/tmp/retrace/build/src/v2/libretrace.so \
          ./build/your-test-binary

      - name: Fuzz short I/O (10% truncation)
        run: |
          cat > /tmp/io.json <<'EOF'
          {
            "intercept_scripts": [
              {
                "func_name": "read",
                "actions": [
                  { "action_name": "call_real" },
                  { "action_name": "incomplete_io",
                    "action_params": { "rate": 0.1 } }
                ]
              },
              {
                "func_name": "write",
                "actions": [
                  { "action_name": "call_real" },
                  { "action_name": "incomplete_io",
                    "action_params": { "rate": 0.1 } }
                ]
              }
            ]
          }
          EOF

          RETRACE_JSON_CONFIG=/tmp/io.json \
          LD_PRELOAD=/tmp/retrace/build/src/v2/libretrace.so \
          ./build/your-test-binary
```

## What this catches

| Failure mode | What it finds |
|--------------|---------------|
| malloc returns NULL | Forgotten NULL checks; crash on OOM. |
| read returns short | Forgotten loop-on-EINTR / partial-read handling. |
| write returns short | Forgotten loop-on-EINTR / partial-write handling. |

A "clean" run exits 0. A buggy run crashes, panics, or hangs — and
the CI step fails. The failure rate (1% / 10%) is low enough that
happy-path tests usually still pass; only error-handling bugs surface.

## Variations

### Deterministic seeds for reproducible failures

If the fuzz finds a bug, you want to reproduce it on your laptop.
Pin the seed:

```yaml
- name: Fuzz malloc (deterministic)
  run: |
    cat > /tmp/fuzz.json <<'EOF'
    {
      "intercept_scripts": [
        {
          "func_name": "*",
          "actions": [
            { "action_name": "fuzzing_seed",
              "action_params": { "seed": 1729 } }
          ]
        },
        {
          "func_name": "malloc",
          "actions": [
            { "action_name": "call_real" },
            { "action_name": "memory_fuzz",
              "action_params": { "fail_rate": 0.01 } }
          ]
        }
      ]
    }
    EOF
    RETRACE_JSON_CONFIG=/tmp/fuzz.json \
    LD_PRELOAD=/tmp/retrace/build/src/v2/libretrace.so \
    ./build/your-test-binary
```

When CI fails, the failure log includes the seed number. Set the
same seed locally and you'll see the same failure — perfect for
filing reproducible bugs.

### Matrix over rates

Test multiple failure intensities in parallel:

```yaml
strategy:
  matrix:
    rate: [0.001, 0.01, 0.05, 0.2]
steps:
  - name: Fuzz at rate=${{ matrix.rate }}
    run: |
      ... use ${{ matrix.rate }} in the JSON ...
```

Low rates catch rare bugs that only fire occasionally; high rates
stress the error paths heavily.

### Only fuzz your test binary, not its dependencies

Use `RETRACE_LOGGER_ALLOWED_FUNCS` to scope interception to a
subset (works as a denylist too):

```yaml
- name: Fuzz only your code
  env:
    RETRACE_LOGGER_ALLOWED_FUNCS: my_alloc,my_read,my_write
  run: ...
```

### Capture the trace on failure

```yaml
- name: Fuzz with trace capture
  run: |
    RETRACE_JSON_CONFIG=/tmp/fuzz.json \
    RETRACE_LOGGER_DEF_STDOUT_ENA=0 \
    RETRACE_LOGGER_DEF_FN=/tmp/trace.json \
    LD_PRELOAD=/tmp/retrace/build/src/v2/libretrace.so \
    ./build/your-test-binary

  - name: Upload trace on failure
    if: failure()
    uses: actions/upload-artifact@v4
    with:
      name: retrace-trace
      path: /tmp/trace.json
```

When CI fails, the trace is attached to the run. Download it,
pretty-print with [tools/logpp](../../tools/logpp/), or render a
[flamegraph](../../tools/flamegraph/) to see where things broke.

## Performance note

Building retrace from source on every CI run adds ~30-60 seconds.
To skip that, publish a binary in your own release artifacts and
download it instead:

```yaml
- name: Install retrace (cached)
  run: |
    curl -L https://github.com/riboseinc/retrace/releases/latest/download/libretrace-linux-x86_64.so \
      -o /tmp/libretrace.so
```

(Once retrace ships per-platform binaries in releases — tracked in
the [packaging README](../../packaging/README.md) — this is the
recommended path.)

## See also

- [09 — Fuzz malloc failures](09-fuzz-malloc.md)
- [10 — Partial I/O (short reads/writes)](10-incomplete-io.md)
- [11 — Deterministic fuzzing seed](11-deterministic-fuzz.md)
- [tools/logpp](../../tools/logpp/) — text pretty-printer
- [tools/flamegraph](../../tools/flamegraph/) — SVG profile
