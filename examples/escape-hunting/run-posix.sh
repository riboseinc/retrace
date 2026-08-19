#!/bin/sh
# Escape-hunting demo, POSIX flow (recipe 33). Steps:
#   1. stage a fake VFS prefix with the declared files
#   2. run the packaged app under retrace (the outside stream)
#   3. correlate the VFS's materialize log against the trace
# Expected output: entry.dat/settings.dat covered, leaked.dat = escape.
#
# usage: run-posix.sh [path-to-build-dir]
set -eu
BUILD=${1:-../../build-test}
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mktemp -d)
PREFIX="$WORK/vfs-root"
mkdir -p "$PREFIX"
echo declared-main > "$PREFIX/entry.dat"
echo declared-vars > "$PREFIX/settings.dat"
echo host-secret > "$PREFIX/leaked.dat"

sed "s|REPLACE_PREFIX|$PREFIX|g" "$HERE/inside.json" > "$WORK/inside.json"

LIB="$BUILD/src/v2/libretrace.so"
CORR="$BUILD/tools/retrace-correlate"
DEMO="$BUILD/examples/escape-demo"
if [ ! -f "$LIB" ]; then
	LIB="$BUILD/src/v2/libretrace.dylib"
fi
if [ ! -f "$LIB" ] || [ ! -x "$CORR" ] || [ ! -x "$DEMO" ]; then
	echo "build tree incomplete under $BUILD (need the library, retrace-correlate, escape-demo); pass the build dir"
	exit 2
fi
case "$(uname)" in
	Darwin) PRELOAD_VAR=DYLD_INSERT_LIBRARIES ;;
	*) PRELOAD_VAR=LD_PRELOAD ;;
esac

echo "== outside stream: the app under retrace"
env "RETRACE_JSON_CONFIG=$HERE/trace-files.json" \
	RETRACE_LOGGER_DEF_ENA=1 RETRACE_LOGGER_DEF_STDOUT_ENA=0 \
	"RETRACE_LOGGER_DEF_FN=$WORK/outside.json" \
	"$PRELOAD_VAR=$LIB" "$DEMO" "$PREFIX"

echo "== correlate"
"$CORR" --inside "$WORK/inside.json" \
	--outside "$WORK/outside.json" --prefix "$PREFIX" || true

rm -rf "$WORK"
