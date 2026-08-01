# Docker integration

retrace ships as a Docker image for zero-config tracing/fuzzing of
containerized applications.

## Quick start: trace any binary

```sh
$ docker run --rm ghcr.io/riboseinc/retrace:latest \
    trace malloc -- /bin/ls
```

## Add tracing to your own container (one line)

```dockerfile
FROM ghcr.io/riboseinc/retrace:latest AS retrace
FROM your-app:latest
COPY --from=retrace /usr/lib/libretrace.so* /usr/lib/
ENV LD_PRELOAD=/usr/lib/libretrace.so
```

Now every libc call in `your-app` is automatically traced.

## Add fuzzing to CI (one line)

```dockerfile
FROM ghcr.io/riboseinc/retrace:latest AS retrace
FROM your-app:latest
COPY --from=retrace /usr/lib/libretrace.so* /usr/lib/
ENV LD_PRELOAD=/usr/lib/libretrace.so
ENV RETRACE_JSON_CONFIG=/etc/retrace/fuzz.json
RUN echo '{"intercept_scripts":[{"func_name":"malloc","actions":[{"action_name":"call_real"},{"action_name":"memory_fuzz","action_params":{"fail_rate":0.01}}]}]}' \
    > /etc/retrace/fuzz.json
```

Every `docker run` of this image now fuzzes malloc at 1% failure rate.
Run your test suite; if it crashes, you found an OOM bug.

## Use cases

| Goal | What to do |
|------|------------|
| See what files your app opens | `ENV RETRACE_JSON_CONFIG=/etc/retrace/trace-open.json` + a config that traces `open` |
| Find OOM bugs | Set `fail_rate` to 0.01 in the fuzz config |
| Profile hot spots | Run under trace config, then `retrace-flamegraph trace.json -o profile.svg` |
| Security audit | Set `RETRACE_LOGGER_ALLOWED_FUNCS=system,execve` to see every command execution |

## Build locally

```sh
$ docker build -t retrace .
$ docker run --rm retrace trace malloc -- /bin/ls
```

## How it works

The image is based on Ubuntu 24.04 with `LD_PRELOAD` set by default.
When you `COPY --from=retrace /usr/lib/libretrace.so* /usr/lib/` into
your app image, the library is loaded into every process. The
`RETRACE_JSON_CONFIG` env var controls which actions run.

The image is ~150MB (Ubuntu base + retrace library + CLI). For a
smaller image, use Alpine as the base (retrace builds on musl).
