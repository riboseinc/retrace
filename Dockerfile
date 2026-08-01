# SPDX-License-Identifier: BSD-2-Clause
#
# Multi-stage Dockerfile that builds retrace and produces a minimal
# runtime image. Consumers COPY --from= this image to add tracing
# to their own containers:
#
#   FROM ghcr.io/riboseinc/retrace:latest AS retrace
#   FROM your-app:latest
#   COPY --from=retrace /usr/lib/libretrace.so /usr/lib/
#   ENV LD_PRELOAD=/usr/lib/libretrace.so
#
# Or use the pre-built image directly to trace any binary:
#
#   docker run --rm ghcr.io/riboseinc/retrace:latest \
#     trace malloc -- /bin/ls

FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    cmake ninja-build gcc g++ pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DRETRACE_BUILD_TESTS=OFF \
    -DRETRACE_BUILD_EXAMPLES=OFF \
    && cmake --build build

# Runtime image: just the library + CLI + default config
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    libc6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/src/v2/libretrace.so* /usr/lib/
COPY --from=builder /src/build/src/cli/retrace /usr/bin/retrace
COPY --from=builder /src/tools/logpp/logpp.py /usr/bin/retrace-logpp
COPY --from=builder /src/tools/flamegraph/flamegraph.py /usr/bin/retrace-flamegraph
RUN chmod +x /usr/bin/retrace-logpp /usr/bin/retrace-flamegraph

# Default config: log every call + call_real (the "trace everything" config)
RUN mkdir -p /etc/retrace && \
    echo '{"intercept_scripts":[{"func_name":"*","actions":[{"action_name":"log_params"},{"action_name":"call_real"}]}]}' \
    > /etc/retrace/default.json

ENV RETRACE_JSON_CONFIG=/etc/retrace/default.json
ENV LD_PRELOAD=/usr/lib/libretrace.so

ENTRYPOINT ["retrace"]
CMD ["--help"]
