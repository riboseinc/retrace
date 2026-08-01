# Docker integration

retrace ships as a Docker image for zero-config tracing/fuzzing of
containerized applications. Three ways to add it to your container:

## Option 1: Direct download (simplest)

No multi-stage build needed. One RUN line downloads the pre-built
library from GitHub releases:

```dockerfile
FROM your-app:latest
RUN apt-get update && apt-get install -y curl \
    && curl -sSL https://github.com/riboseinc/retrace/releases/latest/download/libretrace-linux-x86_64.so \
       -o /usr/lib/libretrace.so \
    && rm -rf /var/lib/apt/lists/*
ENV LD_PRELOAD=/usr/lib/libretrace.so
```

Or with the install script:

```dockerfile
FROM your-app:latest
RUN curl -sSL https://raw.githubusercontent.com/riboseinc/retrace/main/scripts/install.sh | sh
ENV LD_PRELOAD=/usr/local/lib/libretrace.so
```

## Option 2: Multi-stage build (recommended for production)

Copies from the pre-built Docker image — no curl, no download in
the final image:

```dockerfile
FROM ghcr.io/riboseinc/retrace:latest AS retrace
FROM your-app:latest
COPY --from=retrace /usr/lib/libretrace.so* /usr/lib/
ENV LD_PRELOAD=/usr/lib/libretrace.so
```

## Option 3: Run retrace as a container

```sh
$ docker run --rm ghcr.io/riboseinc/retrace:latest \
    trace malloc -- /bin/ls
```

## Add fuzzing to CI (one line)

```dockerfile
FROM your-app:latest
RUN curl -sSL https://github.com/riboseinc/retrace/releases/latest/download/libretrace-linux-x86_64.so \
    -o /usr/lib/libretrace.so
ENV LD_PRELOAD=/usr/lib/libretrace.so
ENV RETRACE_JSON_CONFIG=/etc/retrace/fuzz.json
RUN echo '{"intercept_scripts":[{"func_name":"malloc","actions":[{"action_name":"call_real"},{"action_name":"memory_fuzz","action_params":{"fail_rate":0.01}}]}]}' \
    > /etc/retrace/fuzz.json
```

Every `docker run` now fuzzes malloc at 1% failure rate.

## Use cases

| Goal | Config |
|------|--------|
| See what files your app opens | Trace `open`, `read` |
| Find OOM bugs | `memory_fuzz` with `fail_rate=0.01` |
| Profile hot spots | Trace all, then render flamegraph from JSON log |
| Security audit | Trace `system`, `execve`, `open` |
| Sandbox untrusted code | `sandbox` action with path deny-list |

## Available download URLs

Stable URLs that always point to the latest release:

| Platform | URL |
|----------|-----|
| Linux x86_64 | `https://github.com/riboseinc/retrace/releases/latest/download/libretrace-linux-x86_64.so` |
| Linux aarch64 | `https://github.com/riboseinc/retrace/releases/latest/download/libretrace-linux-aarch64.so` |
| macOS arm64 | `https://github.com/riboseinc/retrace/releases/latest/download/libretrace-macos-arm64.dylib` |
| macOS x86_64 | `https://github.com/riboseinc/retrace/releases/latest/download/libretrace-macos-x86_64.dylib` |

Replace `latest` with a version tag (e.g., `v2.1.0`) for reproducible
builds.

## Build locally

```sh
$ docker build -t retrace .
$ docker run --rm retrace trace malloc -- /bin/ls
```

## Install without Docker

```sh
$ curl -sSL https://raw.githubusercontent.com/riboseinc/retrace/main/scripts/install.sh | sh
$ LD_PRELOAD=/usr/local/lib/libretrace.so /bin/ls
```
