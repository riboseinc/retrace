# retrace benchmark

`bench.sh` measures the wall-clock overhead of running a target
program under retrace vs baseline.

## Why

Before shipping retrace into production tracing / fuzzing pipelines,
you want to know how much overhead it adds. The number depends
heavily on the workload:

- Programs that call libc 10 times: negligible overhead.
- Programs that call libc 100k times per second: substantial.

`bench.sh` gives you the actual number for *your* workload, on
*your* hardware.

## Usage

```sh
$ ./tools/benchmark/bench.sh /path/to/your/program [args...]
target:    /path/to/your/program
library:   /path/to/libretrace.dylib
runs:      10 each configuration
host:      Darwin arm64

running baseline...
running traced...

--- Results ---
median baseline: 0.028s
median traced:   0.045s
overhead:        0.017s (60.7%)
verdict:        REVIEW (>10% overhead; consider per-function filter)
```

## Tuning

| Variable | Default | Effect |
|----------|---------|--------|
| `BENCH_RUNS` | 10 | More runs reduce variance; median is reported. |
| `RETRACE_LIB` | auto-detected | Override the library path. |

## Reducing overhead

If the benchmark shows >10% overhead, narrow the interception
scope:

```sh
# Trace only specific functions, not everything.
$ RETRACE_LOGGER_ALLOWED_FUNCS=open,read,write \
    retrace run --config cookbook/01-trace-all-calls.json -- ./your-program
```

See [docs/cookbook/02-filter-by-function.md](../../docs/cookbook/02-filter-by-function.md).

## Caveats

- The benchmark disables log output (`RETRACE_LOGGER_DEF_ENA=0`)
  to isolate engine + trampoline cost from log I/O cost.
- macOS SIP-protected binaries (`/usr/bin/*`) silently skip
  `DYLD_INSERT_LIBRARIES`; copy the target to `/tmp/` first.
- First run after a reboot may be slower due to cold caches; the
  median of 10 runs mitigates this.
