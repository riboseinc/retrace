#!/bin/sh
# trace-profile-quickstart: macOS flow (TODO.trace-profile/16).
# Same loop as run-linux.sh plus the dtrace kernel-truth layer
# when dtruss is usable (needs SIP off: csrutil disable).
# usage: run-macos.sh [path-to-build-dir]
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-$HERE/../../build-test}
case "$BUILD" in /*) ;; *) BUILD=$(cd "$BUILD" && pwd);; esac
WORK=$(mktemp -d)
cd "$WORK"

printf 'hello\n' > declared.dat
printf 'secret\n' > undeclared.dat
printf 'brand new\n' > new-feature.dat

# the DECLARED set: only declared.dat is allowed; the observed
# trace would allowlist its own escapes. The env read (HOME)
# is NOT declared either -- the jail denies it too (ret NULL).
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

echo "=== 2. validate"
"$BUILD/tools/retrace-profile" validate baseline.json

echo "=== 3. diff the 'upgrade'"
"$BUILD/tools/retrace-profile" capture -o candidate.json -- \
	./app "$WORK" upgraded
"$BUILD/tools/retrace-profile" diff baseline.json candidate.json || true

echo "=== 4. kernel truth via dtruss (best effort; needs SIP off)"
if sudo -n true 2>/dev/null; then
	if sudo dtruss -f ./app > dtruss.log 2>&1; then
		"$BUILD/tools/retrace-dtrace2retrace" \
			-o kernel.json dtruss.log
		"$BUILD/tools/retrace-profile" \
			--libc baseline.json --kernel kernel.json \
			-o graded.json || true
		echo "(kernel layer graded into graded.json)"
	fi
else
	echo "(skipped: dtruss needs sudo + SIP off -- see docs/platforms.md)"
fi

echo "=== 5. jail + jailed run (undeclared.dat DENIED)"
# v1: drop getenv from the jailed set -- jailing env reads with
# a path allowlist segfaults on macOS (TODO.trace-profile/17);
# env visibility stays in the PROFILE, the jail polices paths
sed '/"name": *"getenv"/d' candidate.json > c2.json && mv c2.json candidate.json
"$BUILD/tools/retrace-profile" jail candidate.json \
	--inside inside.json -o jail.json
RETRACE_JSON_CONFIG=jail.json \
	DYLD_INSERT_LIBRARIES="$LIB" \
	./app "$WORK" 2>&1 | tail -5 || true

echo "=== done: $WORK"
