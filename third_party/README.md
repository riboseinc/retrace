# Vendored third-party sources

## otlp-c (third_party/otlp-c)

Pure-C OTLP/HTTP client (OpenTelemetry traces/metrics/logs).
Upstream: https://github.com/riboseinc/otlp-c
Pinned: v0.6.13-2-gdefcd7a (2026-08-24) -- vendored, not
submoduled, so release tarballs stay self-contained (the parson
precedent under src/config/json/).

License: BSD-3-Clause (see third_party/otlp-c/LICENSE; the file
may still carry an older Apache-2.0 header in this checkout --
headers inside src/include already carry
SPDX-License-Identifier: BSD-3-Clause). See THIRD_PARTY_NOTICES
for the notice text.

Refresh: rsync a clean checkout's src/ include/ CMakeLists.txt
LICENSE README.md over this tree, update the pin above, rebuild,
run the otlp converter tests. 0.x API is unstable -- upgrade
deliberately.
