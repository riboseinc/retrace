#!/bin/sh
# fuzz-workbench: corpus -> clustered crash report + reproducers
# (TODO.trace-profile/20). The target (crashy.c) has the classic
# unchecked-malloc bug; memory_fuzz fails allocations; seeds make
# each iteration deterministic; the workbench clusters the crashes
# and emits one reproducer per cluster.
#
# usage: run-posix.sh [path-to-build-dir]
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-$HERE/../../build-test}
case "$BUILD" in /*) ;; *) BUILD=$(cd "$BUILD" && pwd);; esac
WORK=$(mktemp -d)
cd "$WORK"

cc -O0 -g -o crashy "$HERE/crashy.c"

LIB="$BUILD/src/v2/libretrace.so"
[ -f "$LIB" ] || LIB="$BUILD/src/v2/libretrace.dylib"
export RETRACE_V2_LIB="$LIB"

mkdir -p seeds
i=1
while [ $i -le 8 ]; do
	printf 'seed-%d' $i > seeds/s$i
	i=$((i + 1))
done

cat > fuzz.json <<'CONF'
{"intercept_scripts":[{"func_name":"malloc","actions":[
 {"action_name":"log_params"},
 {"action_name":"call_real"},
 {"action_name":"memory_fuzz","action_params":{"fail_rate":0.2}}]}]}
CONF

echo "=== run the corpus"
"$BUILD/tools/retrace-fuzz-report" \
	--config fuzz.json --seeds seeds -o report.json \
	-- ./crashy || true

echo "=== report"
cat report.json

echo "=== reproducers (one per failure cluster)"
ls fuzz-repro-*.json 2>/dev/null || echo "(none)"

echo "=== verify a reproducer replays its crash"
SEED=$(sed -n 's/.*RETRACE_FUZZ_SEED=\([0-9]*\).*/\1/p' \
	$(ls fuzz-repro-*.json | head -1))
if [ -n "$SEED" ]; then
	RETRACE_FUZZ_SEED="$SEED" RETRACE_JSON_CONFIG=fuzz.json \
		LD_PRELOAD="$LIB" DYLD_INSERT_LIBRARIES="$LIB" \
		./crashy >/dev/null 2>&1 && \
		echo "UNEXPECTED: survived" || echo "reproduced: crash"
fi

echo "=== drift oracle (behavior the baseline never saw)"
# baseline: the same target with allocations never failing
cat > base.json <<'CONF'
{"intercept_scripts":[{"func_name":"malloc","actions":[
 {"action_name":"log_params"},
 {"action_name":"call_real"}]}]}
CONF
RETRACE_V2_LIB="$LIB" RETRACE_JSON_CONFIG=base.json \
	"$BUILD/tools/retrace-profile" capture -o baseline.json \
	-- ./crashy >/dev/null 2>&1 || true
if [ -f baseline.json ]; then
	"$BUILD/tools/retrace-fuzz-report" \
		--config base.json --seeds seeds \
		--baseline baseline.json -o drift.json \
		-- ./crashy 2>&1 | grep -a "drift" || true
fi

echo "=== minimized corpus (one reproducer per cluster)"
rm -rf mincorpus
"$BUILD/tools/retrace-fuzz-report" \
	--config fuzz.json --seeds seeds \
	--emit-corpus mincorpus -o report2.json \
	-- ./crashy >/dev/null 2>&1 || true
ls mincorpus 2>/dev/null || echo "(no failures -> empty corpus)"

echo "=== done: $WORK"
