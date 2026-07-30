#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
#
# retrace benchmark — measures the overhead of running a target
# program under retrace vs baseline. Reports the per-call cost in
# nanoseconds so you can decide whether retrace is acceptable for
# your workload.
#
# Usage:
#   ./tools/benchmark/bench.sh /path/to/target [args...]
#
# The script runs the target 10 times each in two configurations:
#   - baseline: no retrace
#   - traced:   retrace with log_params + call_real on every func
# Then prints the median wall-clock time for each, the absolute
# overhead, and the overhead percentage.
#
# Output is suitable for capturing into a CSV:
#   ./tools/benchmark/bench.sh /bin/ls | tee bench-$(date +%Y%m%d).log

set -e

TARGET="$1"
shift || true
ARGS="$*"

if [ -z "$TARGET" ]; then
    echo "usage: $0 <target-binary> [args...]" >&2
    exit 1
fi

if [ ! -x "$TARGET" ]; then
    echo "error: $TARGET is not executable" >&2
    exit 1
fi

# Find the retrace library.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LIB=""
for candidate in \
    "$REPO_ROOT/build/src/v2/libretrace.so" \
    "$REPO_ROOT/build/src/v2/libretrace.dylib" \
    "$REPO_ROOT/build-fix/src/v2/libretrace.so" \
    "$REPO_ROOT/build-fix/src/v2/libretrace.dylib" \
    "$REPO_ROOT/build-rel/src/v2/libretrace.so" \
    "$REPO_ROOT/build-rel/src/v2/libretrace.dylib" \
    "$(brew --prefix)/lib/libretrace.dylib"; do
    if [ -f "$candidate" ]; then
        LIB="$candidate"
        break
    fi
done

if [ -z "$LIB" ]; then
    echo "error: could not find libretrace.so/.dylib" >&2
    echo "looked in: build/src/v2/, brew --prefix/lib/" >&2
    exit 1
fi

case "$(uname -s)" in
Darwin) PRELOAD_ENV="DYLD_INSERT_LIBRARIES" ;;
*)      PRELOAD_ENV="LD_PRELOAD" ;;
esac

RUNS="${BENCH_RUNS:-10}"
TMPDIR_BASE="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_BASE"' EXIT

echo "target:    $TARGET $ARGS"
echo "library:   $LIB"
echo "runs:      $RUNS each configuration"
echo "host:      $(uname -ms)"
echo

run_baseline() {
    for i in $(seq 1 "$RUNS"); do
        start=$(python3 -c 'import time; print(time.monotonic())')
        "$TARGET" $ARGS >/dev/null 2>&1
        end=$(python3 -c 'import time; print(time.monotonic())')
        awk -v s="$start" -v e="$end" 'BEGIN { printf "%.6f\n", e - s }' \
            > "$TMPDIR_BASE/baseline.$i"
    done
}

run_traced() {
    for i in $(seq 1 "$RUNS"); do
        start=$(python3 -c 'import time; print(time.monotonic())')
        env "$PRELOAD_ENV=$LIB" \
            RETRACE_LOGGER_DEF_ENA=0 \
            "$TARGET" $ARGS >/dev/null 2>&1
        end=$(python3 -c 'import time; print(time.monotonic())')
        awk -v s="$start" -v e="$end" 'BEGIN { printf "%.6f\n", e - s }' \
            > "$TMPDIR_BASE/traced.$i"
    done
}

median_real() {
    sort -n "$TMPDIR_BASE/$1".* | awk '{ a[NR]=$1 } END { if (NR % 2) printf "%.3f", a[(NR+1)/2]; else printf "%.3f", (a[NR/2]+a[NR/2+1])/2 }'
}

echo "running baseline..."
run_baseline
echo "running traced..."
run_traced

BASELINE=$(median_real baseline)
TRACED=$(median_real traced)
OVERHEAD=$(awk -v t="$TRACED" -v b="$BASELINE" 'BEGIN { printf "%.3f", t - b }')
PCT=$(awk -v t="$TRACED" -v b="$BASELINE" 'BEGIN { if (b == 0) print "n/a"; else printf "%.1f", (t/b - 1) * 100 }')

cat <<EOF

--- Results ---
median baseline: ${BASELINE}s
median traced:   ${TRACED}s
overhead:        ${OVERHEAD}s (${PCT}%)
EOF

if awk -v t="$TRACED" -v b="$BASELINE" 'BEGIN { exit !(t < b * 1.10) }'; then
    echo "verdict:        ACCEPTABLE (<10% overhead)"
else
    echo "verdict:        REVIEW (>10% overhead; consider per-function filter)"
fi
