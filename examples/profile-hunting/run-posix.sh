#!/bin/sh
# profile-hunting demo, POSIX flow (recipe 34, TODO.windows/08).
# Steps:
#   1. stage a fake VFS prefix + declared manifest (inside.json)
#   2. run the packaged app under retrace (the libc layer)
#   3. if strace exists, also capture the kernel layer and convert
#   4. retrace-profile: profile + claims-vs-truth delta + jail
#   5. run the app again under the jail -- the undeclared read
#      is DENIED at runtime
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

sed "s|REPLACE_PREFIX|$PREFIX|g" "$HERE/../escape-hunting/inside.json" \
	> "$WORK/inside.json"

LIB="$BUILD/src/v2/libretrace.so"
DEMO="$BUILD/examples/escape-demo"
if [ ! -f "$LIB" ]; then
	LIB="$BUILD/src/v2/libretrace.dylib"
fi
if [ ! -f "$LIB" ] || [ ! -x "$DEMO" ] || \
	[ ! -x "$BUILD/tools/retrace-profile" ]; then
	echo "build tree incomplete under $BUILD (need the library, escape-demo, retrace-profile); pass the build dir"
	exit 2
fi
case "$(uname)" in
	Darwin) PRELOAD_VAR=DYLD_INSERT_LIBRARIES ;;
	*) PRELOAD_VAR=LD_PRELOAD ;;
esac

echo "== 1. libc capture (the claims)"
env "RETRACE_JSON_CONFIG=$HERE/../escape-hunting/trace-files.json" \
	RETRACE_LOGGER_DEF_ENA=1 RETRACE_LOGGER_DEF_STDOUT_ENA=0 \
	"RETRACE_LOGGER_DEF_FN=$WORK/outside.json" \
	"$PRELOAD_VAR=$LIB" "$DEMO" "$PREFIX" > /dev/null

KERNEL_ARG=""
if command -v strace >/dev/null 2>&1; then
	echo "== 2. kernel capture (the truth)"
	strace -f -e trace=%file -o "$WORK/strace.log" \
		"$DEMO" "$PREFIX" > /dev/null 2>&1 || true
	"$BUILD/tools/retrace-strace2retrace" "$WORK/strace.log" \
		-o "$WORK/kernel.json" 2>/dev/null
	KERNEL_ARG="--kernel $WORK/kernel.json"
fi

echo "== 3. profile (+ jail from the declared set)"
"$BUILD/tools/retrace-profile" --libc "$WORK/outside.json" \
	$KERNEL_ARG --inside "$WORK/inside.json" \
	--jail-out "$WORK/jail.json" -o "$WORK/profile.json"

echo "== 4. the jailed run (undeclared reads are DENIED)"
env "RETRACE_JSON_CONFIG=$WORK/jail.json" \
	"RETRACE_LOGGER_DEF_STDOUT_ENA=0" \
	"$PRELOAD_VAR=$LIB" "$DEMO" "$PREFIX" || true

echo "== 5. verdicts"
grep -E '"verdict"|"kernel_layer"' "$WORK/profile.json" || true

rm -rf "$WORK"
