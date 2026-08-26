#!/bin/sh
# supervisor-quickstart: the farm loop in one script
# (TODO.supervisor/09, cookbook recipe 37). Platform: Linux.
#   retraced up -> supervise a detonation -> watch events land
#   -> tighten policy mid-run -> freeze -> bundle the journal
# usage: run-linux.sh [path-to-build-dir]
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-$HERE/../../build}
case "$BUILD" in /*) ;; *) BUILD=$(cd "$BUILD" && pwd);; esac
WORK=$(mktemp -d)
cd "$WORK"
trap 'kill $DAEMON_PID 2>/dev/null || true' EXIT

echo "=== 1. supervisor up (nonce published for the spawner)"
"$BUILD/tools/retraced/retraced" \
	--sock agent.sock --journal journal.jsonl \
	--ctl ctl.sock --nonce-file nonce.txt > daemon.log 2>&1 &
DAEMON_PID=$!
i=0
while [ ! -S ctl.sock ]; do
	i=$((i + 1)); [ "$i" -gt 50 ] && { echo "daemon never listened"; exit 1; }
	sleep 0.1
done
NONCE=$(cat nonce.txt)

echo "=== 2. detonate under supervision (denials are events)"
cc -O1 -o app "$HERE/app.c"
cat > boot.json <<EOF
{"intercept_scripts":[{"func_name":"open","actions":[
 {"action_name":"sandbox","action_params":{"deny_paths":["/etc/hosts"]}},
 {"action_name":"call_real"}]}]}
EOF
RETRACE_JSON_CONFIG="$WORK/boot.json" \
RETRACE_SUPERVISOR=1 RETRACE_SUPERVISOR_EAGER=1 \
RETRACE_SUPERVISOR_SOCK="$WORK/agent.sock" \
RETRACE_SUPERVISOR_NONCE="$NONCE" \
RETRACE_LOGGER_DEF_ENA=0 \
LD_PRELOAD="$BUILD/src/v2/libretrace.so" \
	./app 8 > app.out 2>&1 &
APP_PID=$!

echo "=== 3. it registered (poll the registry)"
i=0
while :; do
	N=$("$BUILD/tools/retrace-ctl/retrace-ctl" --sock ctl.sock ps | \
		grep -o '"count":[0-9]*' | head -1 | tr -dc 0-9)
	[ "${N:-0}" -ge 1 ] 2>/dev/null && break
	i=$((i + 1)); [ "$i" -gt 100 ] && { echo "never registered"; exit 1; }
	sleep 0.1
done
"$BUILD/tools/retrace-ctl/retrace-ctl" --sock ctl.sock ps | head -3

echo "=== 4. mid-run tightening (deny writes too)"
cat > tight.json <<EOF
{"policy":{"epoch":2},"intercept_scripts":[{"func_name":"open","actions":[
 {"action_name":"sandbox","action_params":{"deny_paths":["/etc/hosts","$WORK"]}},
 {"action_name":"call_real"}]}]}
EOF
"$BUILD/tools/retrace-ctl/retrace-ctl" --sock ctl.sock policy-push tight.json

echo "=== 5. freeze (the incident-response hold)"
"$BUILD/tools/retrace-ctl/retrace-ctl" --sock ctl.sock freeze
"$BUILD/tools/retrace-ctl/retrace-ctl" --sock ctl.sock status

echo "=== 6. the evidence bundle (hash-chained journal)"
grep -o '"name":"retrace[^"]*"' journal.jsonl | sort | uniq -c | sort -rn | head -8

echo "=== 7. kill --after-ring (end the hold)"
kill $APP_PID 2>/dev/null || true
wait $APP_PID 2>/dev/null || true
kill $DAEMON_PID 2>/dev/null || true
echo "=== done: journal + registry under $WORK (bundle on freeze)"
