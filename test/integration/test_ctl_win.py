#!/usr/bin/env python3
"""
E2E: retrace-ctl on Windows (the fleet CLI over the ctl pipe).

The CLI's whole command surface rides one seam: newline-JSON over
the ctl named pipe. This test drives `status` against a live pipe
daemon and asserts the reply; `ps` proves the JSON surface too.

Usage: test_ctl_win.py <retraced> <retrace-ctl>
"""
import json
import os
import subprocess
import sys
import tempfile
import time


def main():
    if len(sys.argv) != 3:
        print("usage: test_ctl_win.py <retraced> <retrace-ctl>",
              file=sys.stderr)
        return 2
    if os.name != "nt":
        print("SKIP: ctl pipe exists only on Windows", file=sys.stderr)
        return 0
    daemon, ctl = (os.path.abspath(p) for p in sys.argv[1:3])

    work = tempfile.mkdtemp(prefix="ctl-win-")
    agent = "\\\\.\\pipe\\retrace-ctl-e2e-agent"
    ctlpipe = "\\\\.\\pipe\\retrace-ctl-e2e"
    journal = os.path.join(work, "journal.jsonl")

    d = subprocess.Popen(
        [daemon, "--sock", agent, "--ctl", ctlpipe,
         "--journal", journal, "--exit-after", "120"],
        stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    try:
        up = None
        end = time.time() + 10
        while time.time() < end:
            try:
                up = open(ctlpipe, "r+b", buffering=0)
                break
            except OSError:
                time.sleep(0.1)
        if up is None:
            print("FAIL: ctl pipe never appeared", file=sys.stderr)
            return 1
        up.close()

        r = subprocess.run([ctl, "--sock", ctlpipe, "status"],
                           capture_output=True, text=True, timeout=30)
        if r.returncode != 0 or '"ok":1' not in r.stdout:
            print(f"FAIL: status rc={r.returncode}: "
                  f"{r.stdout[:200]} {r.stderr[:200]}", file=sys.stderr)
            return 1
        st = json.loads(r.stdout)
        if "pid" not in st or "policy_epoch" not in st:
            print(f"FAIL: status shape: {r.stdout[:200]}",
                  file=sys.stderr)
            return 1

        r = subprocess.run([ctl, "--sock", ctlpipe, "ps"],
                           capture_output=True, text=True, timeout=30)
        if r.returncode != 0 or '"registry"' not in r.stdout:
            print(f"FAIL: ps rc={r.returncode}: "
                  f"{r.stdout[:200]}", file=sys.stderr)
            return 1

        print("ctl-win: status + ps over the pipe -- OK")
        return 0
    finally:
        if d.poll() is None:
            subprocess.run(["taskkill", "/F", "/T", "/PID",
                            str(d.pid)], capture_output=True)


if __name__ == "__main__":
    sys.exit(main())
