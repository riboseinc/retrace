# fuzz-target: libFuzzer harness template

The persistent in-process harness (TODO.trace-profile/20's
deferred item, now shipped): replace `target_parse` with your
parser; the harness shapes hostile inputs (embedded NUL,
truncation, oversize via the buf cap) and retrace's JSON config
adds libc-level fault injection on top (malloc failure via
memory_fuzz, redirects, mocks) when the harness runs under the
preload.

```sh
clang -g -O1 -fsanitize=fuzzer -o fuzz harness.c
./fuzz corpus/
```

Pair with the workbench: `retrace-fuzz-report --emit-corpus`
produces the minimized reproducing corpus; deterministic
replays need only `RETRACE_FUZZ_SEED`.

Dictionary note: token-dictionary string injection is the
stringinjector tool's job (tools/stringinjector) -- feed its
wordlists there; this harness owns byte-level shaping.

## The custom mutator (`mutator.c`)

`RETRACE_MUTATE_DICT=dictionary.txt` — the same dictionary file
the `fuzz_str` action consumes (one token per line, `#`
comments). The mutator splices tokens at random offsets every
other call, mixing grammar-aware shape with libFuzzer's raw
exploration:

```sh
clang -g -O1 -fsanitize=fuzzer -o fuzz harness.c mutator.c
RETRACE_MUTATE_DICT=dict.txt ./fuzz corpus/
```

The coverage loop closes through `retrace-fuzz-report`: the
workbench clusters findings by call history
(`RETRACE_CALL_HASH`), and `--emit-corpus` minimizes back into
this harness's corpus dir.
