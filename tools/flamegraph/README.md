# retrace-flamegraph

Render a performance profile SVG from a retrace JSON log.

## Why

retrace records per-call wall-clock duration in the log
(`call_duration_us`). The raw JSON is hard to scan visually;
`logpp` produces a text summary; this tool produces a **horizontal
bar chart** of total time spent per libc function — the next best
thing to a true flamegraph for a boundary-intercepting tracer.

## Install

No build step. Python 3.7+.

```sh
$ chmod +x tools/flamegraph/flamegraph.py
$ ln -s "$(pwd)/tools/flamegraph/flamegraph.py" /usr/local/bin/retrace-flamegraph
```

## Usage

```sh
# Capture a trace
$ retrace run --config docs/cookbook/01-trace-all-calls.json \
    --log /tmp/trace.json -- ./your-program

# Render the profile
$ python3 tools/flamegraph/flamegraph.py /tmp/trace.json -o /tmp/profile.svg
wrote /tmp/profile.svg (12 functions)

# Open in browser
$ open /tmp/profile.svg        # macOS
$ xdg-open /tmp/profile.svg    # Linux
```

## Output

Each row is a libc function. Bar width = total time spent in that
function across the whole run. Right-side label shows:

```
<total_ms> (<pct>%) · <count>× · avg <us>µs
```

Sorted by total time, descending. The top bar is your hot spot.

## Caveats

- retrace intercepts at the libc boundary, not at the call site.
  This means you see *which* libc calls are slow, but not *which
  application code* called them. For true call-stack flamegraphs,
  pair with `perf record` or `dtrace`.
- Per-call duration includes the overhead of the retrace engine
  itself. For overhead-sensitive comparisons, use
  [tools/benchmark](../benchmark/) to measure baseline vs traced.
- Time is `CLOCK_MONOTONIC` per call — trustable even when mocking
  `time()` or `gettimeofday()`.

## See also

- [tools/logpp](../logpp/) — text pretty-printer + summary table
- [docs/cookbook/04-time-each-call.md](../../docs/cookbook/04-time-each-call.md)
- [tools/benchmark](../benchmark/) — wall-clock overhead measurement
