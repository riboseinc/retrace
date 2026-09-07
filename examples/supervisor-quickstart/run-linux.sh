#!/bin/sh
# supervisor-quickstart: the farm loop in one script
# (TODO.supervisor/09, cookbook recipe 37). Platform: macOS.
#   retraced up (denial policy at boot) -> retrace-ctl spawns
#   the specimen -> events land -> tighten mid-run -> freeze ->
#   bundle the journal -> kill, with the departure recorded.
# Every step is a control-plane verb: no hand-armed env, no
# shell-side kills.
# usage: run-linux.sh [path-to-build-dir]
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-$HERE/../../build}
case "$BUILD" in /*) ;; *) BUILD=$(cd "$BUILD" && pwd);; esac
WORK=$(mktemp -d)
cd "$WORK"
CTL="$BUILD/tools/retrace-ctl/retrace-ctl"
LIB="$BUILD/src/v2/libretrace.so"
# the daemon's env rides the fork: silence the workload's
# per-call trace logging (the journal is the story, not stdout)
export RETRACE_LOGGER_DEF_ENA=0
trap 'kill $DAEMON_PID 2>/dev/null || true' EXIT

echo "=== 1. supervisor up (the denial policy rides the boot)"
cat > boot.json <<EOF
{"policy":{"epoch":1},"intercept_scripts":[{"func_name":"open","actions":[
 {"action_name":"sandbox","action_params":{"deny_paths":["/etc/hosts"]}},
 {"action_name":"call_real"}]}]}
EOF
"$BUILD/tools/retraced/retraced" \
	--sock agent.sock --journal journal.jsonl \
	--ctl ctl.sock --policy boot.json > daemon.log 2>&1 &
DAEMON_PID=$!
i=0
while [ ! -S ctl.sock ]; do
	i=$((i + 1)); [ "$i" -gt 50 ] && { echo "daemon never listened"; exit 1; }
	sleep 0.1
done

echo "=== 2. the launch arm: spawn (env, nonce, preload -- one journaled command)"
cc -O1 -o app "$HERE/app.c"
REPLY=$("$CTL" --sock ctl.sock spawn --preload "$LIB" -- "$WORK/app" 8)
echo "$REPLY"
APP_PID=$(echo "$REPLY" | sed -n 's/.*"pid":\([0-9]*\).*/\1/p')
[ -n "$APP_PID" ] || { echo "spawn refused"; exit 1; }

echo "=== 3. it joined, and the boot policy bit (epoch >= 1)"
i=0
while :; do
	PS=$("$CTL" --sock ctl.sock ps)
	N=$(echo "$PS" | grep -o '"count":[0-9]*' | head -1 | tr -dc 0-9)
	E=$(echo "$PS" | grep -o '"policy_epoch":[0-9]*' | head -1 | tr -dc 0-9)
	[ "${N:-0}" -ge 1 ] 2>/dev/null && [ "${E:-0}" -ge 1 ] 2>/dev/null && break
	i=$((i + 1)); [ "$i" -gt 150 ] && { echo "never registered/policied"; exit 1; }
	sleep 0.1
done
"$CTL" --sock ctl.sock ps | head -3
# two beats in the denied window before the story moves on
sleep 2

echo "=== 4. mid-run tightening (deny writes too)"
cat > tight.json <<EOF
{"policy":{"epoch":2},"intercept_scripts":[{"func_name":"open","actions":[
 {"action_name":"sandbox","action_params":{"deny_paths":["/etc/hosts","$WORK"]}},
 {"action_name":"call_real"}]}]}
EOF
"$CTL" --sock ctl.sock policy-push tight.json

echo "=== 5. freeze (the incident-response hold)"
"$CTL" --sock ctl.sock freeze
"$CTL" --sock ctl.sock status

echo "=== 6. the evidence bundle (hash-chained journal)"
grep -o '"name":"retrace[^"]*"' journal.jsonl | sort | uniq -c | sort -rn | head -8

echo "=== 7. kill (the departure is journaled too)"
"$CTL" --sock ctl.sock kill "$APP_PID"
i=0
while kill -0 "$APP_PID" 2>/dev/null; do
	i=$((i + 1)); [ "$i" -gt 40 ] && break
	sleep 0.1
done
grep -o '"name":"retrace.ctl[^"]*"' journal.jsonl | tail -3

echo "=== 8. the complete bundle (graceful close flushes the tail)"
kill $DAEMON_PID 2>/dev/null || true
wait $DAEMON_PID 2>/dev/null || true
grep -o '"name":"retrace[^"]*"' journal.jsonl | sort | uniq -c | sort -rn | head -10
echo "=== done: journal + registry under $WORK (bundle on freeze)"
