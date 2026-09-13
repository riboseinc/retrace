#!/usr/bin/env python3
"""
E2E: campaign orchestration (TODO.impl/09). A 3x2 fixture
campaign -- three samples (clean, failing, crashing) x two
policies -- runs against a real daemon. The card's acceptance,
verbatim:

  - 6 sessions: 6 launches land in the journal, each with its
    departure (the reap doctrine)
  - indexed evidence: each run's summary line names a working
    journal query for its pid
  - the triage table: all six cells report verdicts (CLEAN,
    FAIL, CRASH -- the samples' designed outcomes)

Usage: test_campaign.py <retraced> <retrace-ctl> <retrace-campaign> <lib>
"""
import json
import os
import subprocess
import sys
import tempfile
import time

SAMPLES = {
    "clean": 'int main(void){return 0;}',
    "failing": 'int main(void){return 7;}',
    "crashing": 'int main(void){int *p=0;*p=1;return 0;}',
}

POLICIES = {
    "permissive": {"policy": {"epoch": 1},
                   "intercept_scripts": [
                       {"func_name": "*",
                        "actions": [{"action_name": "log_params"},
                                    {"action_name": "call_real"}]}]},
    "strict": {"policy": {"epoch": 1},
               "intercept_scripts": [
                   {"func_name": "*",
                    "actions": [{"action_name": "log_params"},
                                {"action_name": "call_real"}]}]},
}


def main():
    if len(sys.argv) != 5:
        print("usage: test_campaign.py <retraced> <retrace-ctl> "
              "<retrace-campaign> <lib>", file=sys.stderr)
        return 2
    if not sys.platform.startswith("linux") and \
            sys.platform != "darwin":
        print("SKIP: POSIX-only daemon")
        return 0

    retraced, ctl, campaign, lib = (os.path.abspath(a)
                                    for a in sys.argv[1:])
    work = tempfile.mkdtemp(prefix="campaign-")
    nonce = "b" * 32
    base = os.path.join(work, "journal.jsonl")

    for name, src in SAMPLES.items():
        c = os.path.join(work, f"{name}.c")
        with open(c, "w") as f:
            f.write(src)
        b = subprocess.run(["cc", "-o", os.path.join(work, name), c],
                           capture_output=True)
        if b.returncode != 0:
            print(f"FAIL: sample {name} build failed",
                  file=sys.stderr)
            return 1

    pol_paths = {}
    for name, pol in POLICIES.items():
        p = os.path.join(work, f"{name}.json")
        with open(p, "w") as f:
            json.dump(pol, f)
        pol_paths[name] = p

    daemon = subprocess.Popen(
        [retraced, "--sock", os.path.join(work, "a.sock"),
         "--ctl", os.path.join(work, "c.sock"),
         "--journal", base, "--policy",
         pol_paths["permissive"], "--nonce", nonce],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.0)

    manifest = {
        "ctl_sock": os.path.join(work, "c.sock"),
        "journal": base,
        "preload": lib,
        "repeats": 1,
        "concurrency": 2,
        "timeout_sec": 20,
        "samples": [{"name": n, "argv": [os.path.join(work, n)]}
                    for n in SAMPLES],
        "policies": [{"name": n, "path": pol_paths[n]}
                     for n in POLICIES],
    }
    mpath = os.path.join(work, "manifest.json")
    with open(mpath, "w") as f:
        json.dump(manifest, f)

    env = dict(os.environ)
    env["PATH"] = os.path.dirname(ctl) + os.pathsep + env["PATH"]
    r = subprocess.run([campaign, mpath], capture_output=True,
                       text=True, env=env, timeout=120)
    daemon.terminate()
    daemon.wait(timeout=10)
    time.sleep(0.5)

    out = r.stdout
    if "= 6 runs" not in out:
        print(f"FAIL: campaign did not expand 6 cells:\n{out}",
              file=sys.stderr)
        return 1
    for want in ("CLEAN", "FAIL", "CRASH"):
        if want not in out:
            print(f"FAIL: verdict {want} missing:\n{out}",
                  file=sys.stderr)
            return 1
    # each sample under BOTH policies: 2 CLEAN, 2 FAIL, 2 CRASH
    if out.count("CLEAN") != 2 or out.count(" CRASH") != 2:
        print(f"FAIL: verdict counts wrong:\n{out}", file=sys.stderr)
        return 1
    print("triage: 6 cells, verdicts CLEAN/FAIL/CRASH x2 each")

    segs = sorted(f for f in os.listdir(work)
                  if f.startswith("journal.jsonl"))
    blob = ""
    for s in segs:
        with open(os.path.join(work, s), "r", errors="replace") as f:
            blob += f.read()
    if blob.count("retrace.ctl.spawn") != 6:
        print(f"FAIL: journal carries "
              f"{blob.count('retrace.ctl.spawn')} launches (want 6)",
              file=sys.stderr)
        return 1
    if blob.count("retrace.ctl.exit") != 6:
        print(f"FAIL: journal carries "
              f"{blob.count('retrace.ctl.exit')} departures "
              f"(want 6)", file=sys.stderr)
        return 1
    print("journal: 6 launches + 6 departures (the reap doctrine)")

    # indexed evidence: the query line from the first row works
    import re
    m = re.search(r"--query 'pid == (\d+)'", out)
    if m is None:
        print("FAIL: no evidence query line in the table",
              file=sys.stderr)
        return 1
    pid = m.group(1)
    q = subprocess.run([ctl, "events", "--journal", base,
                        "--query", f'pid == {pid}'],
                       capture_output=True, text=True)
    if q.returncode != 0 or f'"pid":{pid}' not in q.stdout:
        print(f"FAIL: evidence query for pid {pid} empty",
              file=sys.stderr)
        return 1
    print("evidence: the table's per-run query returns records")

    print("PASS: campaign orchestration holds end to end")
    return 0


if __name__ == "__main__":
    sys.exit(main())
