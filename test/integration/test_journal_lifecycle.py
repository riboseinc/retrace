#!/usr/bin/env python3
"""
E2E: the journal lifecycle (TODO.impl/10) -- rotation,
retention, and query, driven through the real daemon and ctl.

A daemon with a small rotation cap and a tight byte budget
receives a stream of policy pushes. The assertions are the
card's acceptance, verbatim:

  - rotation happened: a numbered segment series exists
  - verifiability holds ACROSS segments (verify-journal walks
    the series; the oldest surviving segment may reference a
    pruned predecessor -- recorded, never silent)
  - a query over the segments returns the same set as grep
  - retention pruned the oldest segments (segment 0000 is
    gone) and the prune is a chained record

Usage: test_journal_lifecycle.py <retraced> <retrace-ctl>
"""
import json
import os
import subprocess
import sys
import tempfile
import time


def main():
    if len(sys.argv) != 3:
        print("usage: test_journal_lifecycle.py <retraced> "
              "<retrace-ctl>", file=sys.stderr)
        return 2
    if not sys.platform.startswith("linux") and \
            sys.platform != "darwin":
        print("SKIP: POSIX-only daemon")
        return 0

    retraced = os.path.abspath(sys.argv[1])
    ctl = os.path.abspath(sys.argv[2])
    work = tempfile.mkdtemp(prefix="jlife-")
    base = os.path.join(work, "journal.jsonl")
    policy = os.path.join(work, "pol.json")
    nonce = "a" * 32

    with open(policy, "w") as f:
        json.dump({"policy": {"epoch": 1},
                   "intercept_scripts": [
                       {"func_name": "*",
                        "actions": [
                            {"action_name": "log_params"},
                            {"action_name": "call_real"}]}]},
                  f)

    daemon = subprocess.Popen(
        [retraced, "--sock", os.path.join(work, "a.sock"),
         "--ctl", os.path.join(work, "c.sock"),
         "--journal", base, "--policy", policy,
         "--nonce", nonce,
         "--journal-rotate-bytes", "700",
         "--journal-budget-bytes", "4000"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.0)

    ctl_sock = ["--sock", os.path.join(work, "c.sock")]
    for _ in range(25):
        subprocess.run([ctl] + ctl_sock + ["policy-push", policy],
                       stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
        time.sleep(0.08)

    daemon.terminate()
    daemon.wait(timeout=10)
    time.sleep(0.5)

    segs = sorted(f for f in os.listdir(work)
                  if f.startswith("journal.jsonl."))
    if len(segs) < 3:
        print(f"FAIL: rotation produced {len(segs)} segments "
              f"(want >= 3)", file=sys.stderr)
        return 1
    print(f"rotation: {len(segs)} segments survive")

    v = subprocess.run([ctl, "verify-journal", base],
                       capture_output=True, text=True)
    if v.returncode != 0 or "segments verified" not in v.stdout:
        print(f"FAIL: verify-journal rc={v.returncode}:\n"
              f"{v.stdout}\n{v.stderr}", file=sys.stderr)
        return 1
    print("verify: series verifies across segments "
          "(chain + cross-links)")

    q = subprocess.run(
        [ctl, "events", "--journal", base,
         "--query", 'name ~ "*policy*"'],
        capture_output=True, text=True)
    matched = q.stdout.count("policy")
    grep = 0
    for s in segs:
        with open(os.path.join(work, s), "r",
                  errors="replace") as f:
            grep += f.read().count("policy")
    if matched == 0 or matched != grep:
        print(f"FAIL: query matched {matched}, grep found "
              f"{grep}", file=sys.stderr)
        return 1
    print(f"query: {matched} records == grep over the series")

    first = os.path.join(work, "journal.jsonl.0000")
    if os.path.exists(first):
        print("FAIL: retention never pruned segment 0000",
              file=sys.stderr)
        return 1
    pruned = any("segment_pruned" in open(
        os.path.join(work, s), "r", errors="replace").read()
        for s in segs)
    if not pruned:
        print("FAIL: no chained prune record in the survivors",
              file=sys.stderr)
        return 1
    print("retention: oldest pruned, records chained")

    print("PASS: journal lifecycle holds end to end")
    return 0


if __name__ == "__main__":
    sys.exit(main())
