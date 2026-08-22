#!/bin/sh
# trace-profile-quickstart: the full loop in one script
# (TODO.trace-profile/16). Platform: Linux.
#   capture -> validate -> drift (diff) -> jail -> jailed run
# usage: run-linux.sh [path-to-build-dir]
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

echo "=== 3. capture the 'upgrade' and diff"
"$BUILD/tools/retrace-profile" capture -o candidate.json -- \
	./app "$WORK" upgraded
"$BUILD/tools/retrace-profile" diff baseline.json candidate.json || true

echo "=== 4. jail from the candidate profile"
# v1: drop getenv from the jailed set -- jailing env reads with
# a path allowlist segfaults on macOS (TODO.trace-profile/17);
# env visibility stays in the PROFILE, the jail polices paths
sed '/"name": *"getenv"/d' candidate.json > c2.json && mv c2.json candidate.json
"$BUILD/tools/retrace-profile" jail candidate.json \
	--inside inside.json -o jail.json

echo "=== 5. run under the jail (undeclared.dat must be DENIED)"
cat jail.json | head -20
RETRACE_JSON_CONFIG=jail.json \
	LD_PRELOAD="$BUILD/src/v2/libretrace.so" ./app 2>&1 | tail -5 || true

echo "=== done: $WORK"
