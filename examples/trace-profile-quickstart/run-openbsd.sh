#!/bin/sh
# trace-profile-quickstart: OpenBSD/NetBSD flow
# (TODO.trace-profile/16 + 24 follow-up). Same loop with the
# ktrace/kdump kernel-truth layer (ktrace2retrace, v2.23.0).
# usage: run-openbsd.sh [path-to-build-dir]
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-$HERE/../../build-test}
case "$BUILD" in /*) ;; *) BUILD=$(cd "$BUILD" && pwd);; esac
WORK=$(mktemp -d)
cd "$WORK"

printf 'hello\n' > declared.dat
printf 'secret\n' > undeclared.dat
printf 'brand new\n' > new-feature.dat

# the DECLARED set: only declared.dat is allowed
cat > inside.json <<EOF
{"profile":{"functions":[{"name":"fopen","count":1}],
 "accesses":[{"path":"$WORK/declared.dat","class":"read","hits":1}]}}
EOF

cc -O1 -o app "$HERE/app.c"

LIB=$( [ -f "$BUILD/src/v2/libretrace.so" ] && \
	echo "$BUILD/src/v2/libretrace.so" || \
	echo "$BUILD/src/v2/libretrace.dylib" )
export RETRACE_V2_LIB="$LIB"

echo "=== 1. capture (baseline)"
"$BUILD/tools/retrace-profile" capture -o baseline.json -- ./app "$WORK"

echo "=== 2. diff the 'upgrade'"
"$BUILD/tools/retrace-profile" capture -o candidate.json -- \
	./app "$WORK" upgraded
"$BUILD/tools/retrace-profile" diff baseline.json candidate.json || true

echo "=== 3. kernel truth via ktrace/kdump"
# OpenBSD: ktrace -i -f kd.out ./app; kdump -f kd.out
# NetBSD:  same pair; kdump prints NAMI (resolved paths) lines
if command -v ktrace >/dev/null 2>&1 &&
   command -v kdump >/dev/null 2>&1; then
	ktrace -i -f kd.out ./app || true
	kdump -f kd.out > kdump.log || true
	"$BUILD/tools/retrace-ktrace2retrace" -o kernel.json kdump.log
	"$BUILD/tools/retrace-profile" \
		--libc baseline.json --kernel kernel.json \
		-o graded.json || true
	echo "(kernel layer graded into graded.json)"
else
	echo "(ktrace/kdump not found)"
fi

echo "=== 4. jail + jailed run (undeclared.dat DENIED)"
sed '/"name": *"getenv"/d' candidate.json > c2.json && mv c2.json candidate.json
"$BUILD/tools/retrace-profile" jail candidate.json \
	--inside inside.json -o jail.json
RETRACE_JSON_CONFIG=jail.json \
	LD_PRELOAD="$BUILD/src/v2/libretrace.so" ./app 2>&1 | tail -5 || true

echo "=== done: $WORK"
