# Recipe 35: dictionary-driven string fuzzing (fuzz_str)

`memory_fuzz` fails allocations; `fuzz_str` explores CONTENT:
every call replaces a string parameter with a token drawn from
an AFL-style dictionary. Deterministic per seed — a reproducer
is config + seed + dict.

## The dictionary

One token per line; `#` lines and blanks are skipped (see
`test/fixtures/fuzz-dict.txt` for the tested shape):

```
# paths a parser should survive
/etc/passwd
../../../etc/shadow
%s%s%s%n
https://example.invalid/callback
```

## The config

```json
{ "intercept_scripts": [{
    "func_name": "fopen",
    "actions": [
      { "action_name": "fuzz_str",
        "action_params": {
          "param_name": "filename",
          "dict": "dict.txt",
          "match_str": "/etc/hosts",
          "fuzz_seed": 42 } },
      { "action_name": "log_params" },
      { "action_name": "call_real" } ] }] }
```

`fuzz_str` runs BEFORE `log_params` so the trace records the
token the callee actually received. `match_str` (optional) gates
the fuzzing to calls carrying a given value — fuzz only the
config file, leave everything else alone.

## Run it

```sh
RETRACE_JSON_CONFIG=fuzz.json LD_PRELOAD=.../libretrace.so \
    ./app /etc/hosts
```

Same `fuzz_seed` (or `RETRACE_FUZZ_SEED`) + same dict = same
token sequence, every run. Change the seed to explore.

## End-to-end demo

`examples/fuzz-workbench` runs the whole flow: three runs, token
sequences extracted from the trace logs, byte-compared — the
reproducibility promise shown, not claimed
(`run-posix.sh`, the dictionary section).
