# fuzz-workbench: corpus -> clustered crash report + reproducers

The fuzzing-workbench flow (TODO.trace-profile/20): a corpus of
seeds drives a target under a fuzz config; `retrace-fuzz-report`
classifies each iteration (crash / assertion / clean), clusters
failures by (last-called function, param count), and emits one
REPRODUCER per cluster -- the same config plus the
`RETRACE_FUZZ_SEED` value that produced the failure.

```sh
./run-posix.sh [build-dir]
```

The demo target (`crashy.c`) has the classic unchecked-malloc
bug; `memory_fuzz` fails allocations deterministically per
seed. Expected: several crashes, all in one or two clusters
("malloc", and "?" for deaths before any trace entry flushed),
each with a reproducer that replays its crash.

The pieces:

| Piece | What |
|---|---|
| RETRACE_FUZZ_SEED env | makes any memory_fuzz config deterministically re-drivable (no config edits) |
| retrace-fuzz-report | corpus runner + clustering + repro emission; exit 1 on crashes (CI-able) |
| report.json | the shape documented in docs/reports.md |

Assertion mode: pass `--marker <substring>` to classify
non-signal failures by a marker string in the trace.

## Dictionary fuzzing (fuzz_str)

The same runner also demos CONTENT fuzzing: `fuzz_str` replaces
`fopen`'s `filename` with tokens from `dict.txt` (AFL-style:
one per line, `#` comments skipped), deterministically per
`fuzz_seed`. `@`-prefixed lines are grammar TEMPLATES: `%N%`
substitutes the Nth flat token -- e.g. `@GET %2% HTTP/1.1`
builds structured payloads from parts (docs/configuration.md). The demo runs the target three times and
byte-compares the token sequence from the trace logs -- the
reproducibility promise (reproducer = config + seed + dict).
See `paths.c` and the dictionary section in `run-posix.sh`;
action reference in `docs/configuration.md`.
