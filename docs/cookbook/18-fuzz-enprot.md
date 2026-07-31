# 18 — Fuzz enprot's EPT parser

## Problem

[enprot](https://github.com/engyon/enprot) is the reference CLI for
the Engyon Protected Text (EPT) format — Ribose Standard RSD 12001.
It processes untrusted input files (`.ept` documents from external
collaborators), so the parser is a natural fuzzing target.

retrace can fuzz enprot without modifying the Rust source or writing
a libFuzzer harness: intercept the libc calls enprot makes during
parsing and inject failures at configurable rates.

## Prerequisites

```sh
$ git clone https://github.com/engyon/enprot
$ cd enprot
$ cargo build --release
$ # the binary is at target/release/enprot
```

This recipe targets the `passthrough` subcommand — it parses an EPT
file end-to-end without applying any transform (no encryption, no
signing). That isolates parser cost from crypto cost.

## Recipe A — Audit file access

First, see what files enprot touches when parsing untrusted input.
This is a security audit: are there surprise reads (config, history,
keys)?

`audit-enprot-files.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "open",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    },
    {
      "func_name": "openat",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    },
    {
      "func_name": "read",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

### Invocation

```sh
$ cat > /tmp/sample.ept <<'EOF'
// <( BEGIN Secret )>
hello, world
// <( END Secret )>
EOF

$ retrace run \
    --config docs/cookbook/audit-enprot-files.json \
    --log /tmp/enprot-trace.json \
    -- ./enprot/target/release/enprot passthrough /tmp/sample.ept
$ python3 tools/logpp/logpp.py /tmp/enprot-trace.json | grep open
[INFO] FUNCS  open(path=/tmp/sample.ept ...) ret=3
[INFO] FUNCS  openat(dirfd=AT_FDCWD path=/etc/localtime ...) ret=4
```

The `/etc/localtime` read is timezone lookup; the only *content*
read is the input file. Audit-clean.

## Recipe B — Fuzz malloc during parse

EPT parsing allocates heavily (AST nodes, byte buffers for CAS
fetches). Inject OOM at a 1% rate and watch how enprot's error
handling responds.

`fuzz-enprot-malloc.json`:

```json
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
```

### Invocation

```sh
$ retrace run \
    --config docs/cookbook/fuzz-enprot-malloc.json \
    -- ./enprot/target/release/enprot passthrough /tmp/sample.ept
# most runs: enprot reports an error and exits cleanly
# some runs: enprot panics — that's a bug worth filing
```

### Find the failure mode

Capture stderr to see whether enprot handles OOM gracefully (returns
`Err`) or panics (Rust `panic!` unwinds):

```sh
$ retrace run --config docs/cookbook/fuzz-enprot-malloc.json -- \
    ./enprot/target/release/enprot passthrough /tmp/sample.ept 2>&1 \
    | grep -E "error|panic|ENOMEM" | head -3
```

If you see `panic`, the parser doesn't propagate allocation failures
cleanly. File an issue on enprot with the panic trace + this config
attached; the maintainer can reproduce exactly by pinning
`fuzzing_seed`.

### Make it reproducible

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
      "actions": [ ... ]
    },
    {
      "func_name": "calloc",
      "actions": [ ... ]
    }
  ]
}
```

Pin the seed and the same allocation will fail on every run —
perfect for filing reproducible bugs.

## Recipe C — Fuzz read length (parse robustness)

EPT files can be huge. The parser must handle short reads from disk
without corrupting state. Use `incomplete_io` to force short reads
on every `read()` call.

`fuzz-enprot-read.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "read",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "incomplete_io",
          "action_params": { "rate": 0.1 } }
      ]
    }
  ]
}
```

Each `read()` returns 10% of the bytes actually available. If the
parser concatenates correctly, output is unchanged. If it panics or
produces a corrupt AST, you've found a parsing bug.

```sh
$ retrace run \
    --config docs/cookbook/fuzz-enprot-read.json \
    -- ./enprot/target/release/enprot passthrough /tmp/sample.ept
```

Compare the parsed output (stdout) against a baseline run:

```sh
$ ./enprot/target/release/enprot passthrough /tmp/sample.ept > /tmp/baseline.ept
$ retrace run --config docs/cookbook/fuzz-enprot-read.json -- \
    ./enprot/target/release/enprot passthrough /tmp/sample.ept > /tmp/fuzzed.ept
$ diff /tmp/baseline.ept /tmp/fuzzed.ept
```

No diff = parser is robust to short reads. Diff = bug.

## Recipe D — Capture I/O during verify-chain

`enprot verify-chain` walks the CHAIN anchor DAG, which involves
signature verification (librnp → libc). Capture the libc surface to
understand the trust path:

```json
{
  "intercept_scripts": [
    { "func_name": "open",   "actions": [
        { "action_name": "log_params" }, { "action_name": "call_real" } ] },
    { "func_name": "read",   "actions": [
        { "action_name": "log_params" }, { "action_name": "call_real" } ] },
    { "func_name": "malloc", "actions": [
        { "action_name": "log_params" }, { "action_name": "call_real" } ] },
    { "func_name": "getrandom", "actions": [
        { "action_name": "log_params" }, { "action_name": "call_real" } ] }
  ]
}
```

This shows which files supply the keys, which supply the data being
verified, and where randomness enters (important: signature
verification should be deterministic; randomness here would be a
red flag).

## Going further

### Scale to a corpus

Wrap in a shell loop over a corpus of EPT files:

```sh
$ for f in corpus/*.ept; do
    retrace run --config docs/cookbook/fuzz-enprot-malloc.json -- \
        ./enprot/target/release/enprot passthrough "$f" >/dev/null 2>&1 \
        || echo "FAIL: $f"
  done
```

### Compare with `cargo fuzz`

enprot ships libFuzzer harnesses in `fuzz/fuzz_targets/`. Those
find parser bugs by mutating input bytes. retrace finds bugs by
mutating the *environment* (allocation success, I/O completion).
The two approaches are complementary: libFuzzer finds parser logic
bugs; retrace finds error-handling and resource-exhaustion bugs.

### Pin to the engyon site

The [engyon cookbook](https://www.engyon.org/cookbook/) lists
recipes for working with EPT documents. This recipe is the inverse
direction: stress-testing the EPT *processor* from outside. Consider
contributing a link from the engyon site back here once you've filed
your first enprot bug found via retrace.

## See also

- [09 — Fuzz malloc failures](09-fuzz-malloc.md)
- [10 — Partial I/O (short reads/writes)](10-incomplete-io.md)
- [11 — Deterministic fuzzing seed](11-deterministic-fuzz.md)
- [13 — Audit system() for injection](13-audit-system.md)
- [enprot documentation](https://docs.rs/enprot)
- [Engyon Protected Text specification (RSD 12001)](https://github.com/riboseinc/rsd-engyon-syntax)
