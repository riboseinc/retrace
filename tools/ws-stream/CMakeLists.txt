# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-ws: real-time WebSocket streamer for retrace's JSON log
# (TODO.complete/22 MVP).
#
# Tails the JSON log file written by retrace (one entry per array
# element, comma-separated, wrapped in `[` ... `]`), parses each
# entry as it lands, and broadcasts it to every connected
# WebSocket client.
#
# Standalone Python -- no engine change, no runtime overhead on
# the target. Pairs naturally with the lock-free logger (TODO 19)
# for high-volume tracing, but works fine with the existing
# mutex-based logger too.
#
# Usage:
#   retrace run --log /tmp/trace.json -- ./your-binary &
#   retrace-ws /tmp/trace.json
#   # Open http://localhost:8765/ in a browser to see live events
#
# Requires: websockets (`pip install websockets`).

install(PROGRAMS retrace-ws
    DESTINATION bin)
