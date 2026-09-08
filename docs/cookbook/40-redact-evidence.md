# 40 — Redact secrets from evidence

## Problem

Everything retrace observes becomes evidence: dereferenced
string parameters (a path with `?token=…`, an env name), logged
string arguments, captured buffers. If that evidence ships to
an otelcol or lands in a journal a compliance team reads, the
secrets ship with it.

## Config

The `redact` array — patterns of tokens that never leave the
process:

```json
{
  "redact": ["*_TOKEN*", "AWS_*", "token=x96*", "password=*"],
  "intercept_scripts": [ ... ]
}
```

`RETRACE_REDACT="*_TOKEN,AWS_*"` (comma-separated) does the
same for config-less runs.

## Semantics

A pattern matches **whole tokens**: a maximal run of
`[A-Za-z0-9_=.:+@%~-]` — the shapes secrets take. `*` is a run
of any characters. `=` is *inside* tokens, so a `key=value`
pair redacts as one unit: `"AWS_*"` removes
`AWS_SECRET_ACCESS_KEY=aksk…` entire. Delimiters (`/ ? &`,
whitespace) bound the replacement: `"?token=x96_file"` becomes
`"?***"`.

One transform runs at the evidence fan-out — the logger's emit
(stdout, logfile, every sink — the OTLP streamer rides the same
seam) and the supervisor agent's event builder — so every
destination inherits, and with no patterns compiled the cost is
one branch.

## Invocation

```sh
RETRACE_JSON_CONFIG=redact.json DYLD_INSERT_LIBRARIES=libretrace.dylib \
    ./app 2> evidence.jsonl
# evidence.jsonl: "*path": ["\/tmp\/\*\*\*"], no token anywhere
```

## Notes

- Evidence is JSON: `/` serializes as `\/` — patterns never
  need path separators (token/key shapes don't contain them).
- Redaction is evidence-only: the target's own stdout is
  untouched (it printed the secret itself; that's its behavior,
  not your evidence).
