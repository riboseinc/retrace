# supervisor-compose: the whole story, one `up` (TODO.impl/16)

The reference stack: otelcol, the daemon, and a supervised
detonation whose denied reads land live in the collector and
chained in the journal.

```sh
# a LINUX build tree (containers need ELF; a macOS tree is Mach-O):
docker run --rm -v "$PWD/../..":/src:ro -v "$PWD/../../build-compose":/out \
  ubuntu:24.04 sh -c 'apt-get update -qq && apt-get install -y -qq \
  build-essential cmake ninja-build >/dev/null && \
  cmake -S /src -B /out -G Ninja -DRETRACE_BUILD_TESTS=OFF \
  -DRETRACE_BUILD_EXAMPLES=OFF >/dev/null && \
  cmake --build /out --target retraced retrace_ctl retrace_v2'

export RETRACE_BUILD="$PWD/../../build-compose"
export RETRACE_NONCE=0123456789abcdef0123456789abcdef   # fixed: reproducible
docker compose up -d

# the fleet, live:
docker compose exec retraced /tmp/retrace-ctl --sock /sockets/c.sock ps
docker compose exec retraced /tmp/retrace-ctl --sock /sockets/c.sock drift

# the evidence (denials are durable-class: read after a graceful stop):
docker compose stop retraced
docker compose run --rm --no-deps retraced \
  sh -c 'grep retrace.jail.denied /evidence/journal.jsonl | head'

docker compose down -v
```

`docker compose logs otelcol` is the LIVE security feed -- the
debug exporter prints each `retrace.jail.denied` body as it
arrives (OTLP/HTTP on 4318).

## How it fits together

- The shared `sockets` volume carries the agent socket across
  containers: the specimen joins the daemon with the FULL
  nonce (never a spectator), exactly as `spawn` would arrange.
- The daemon boots with `--policy policy.json` (deny
  `/etc/shadow`); the specimen is `tail --retry -F` -- ONE
  long-lived process that re-opens the denied path in-process
  forever. Sub-second fork-loops race the eager agent's
  connect; the initial `-F` open is honestly pre-policy (the
  first read succeeds, every retry after denies).
- The journal volume keeps the hash chain between runs' eyes:
  `docker compose run --rm retraced retrace-ctl verify-journal
  /evidence/journal.jsonl --pubkey ...` extends to signing
  when the daemon gets `--signing-key`.

## Platform notes

Containers must match the build tree's architecture (an arm64
Mac build needs `platform: linux/arm64` services -- or build
the tree in Docker as above, which matches by construction).
The compose file itself is arch-neutral; CI validates it with
`docker compose config` (test_compose.py self-skips without a
compose binary).
