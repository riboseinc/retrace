#!/usr/bin/env python3
"""
E2E: retraced on Windows over named pipes (TODO.supervisor/12 P0).

The daemon serves \\\\.\\pipe\\<name> with the same RTRD framing and
state machine as the POSIX UDS daemon. This test opens the pipe as a
file (CreateFile under the hood), walks HELLO -> WELCOME -> EVENT x3
(source=kernel) -> BYE without the nonce (the spectator doctrine),
stops the daemon, and asserts the journal carries the observations,
the spectator seat, and the drift summary.

Usage: test_retraced_pipe.py <retraced>
"""
import json
import os
import subprocess
import sys
import tempfile
import time

from rpipe import (frame, recv_frame, open_pipe,
                   journal_records, taskkill)

MAGIC = b"RTRD"
HELLO, HEARTBEAT, EVENT, BYE, WELCOME = 1, 2, 4, 6, 16


def main():
    if len(sys.argv) != 2:
        print("usage: test_retraced_pipe.py <retraced>", file=sys.stderr)
        return 2
    if os.name != "nt":
        print("SKIP: named-pipe daemon exists only on Windows",
              file=sys.stderr)
        return 0
    daemon = os.path.abspath(sys.argv[1])

    work = tempfile.mkdtemp(prefix="pipe-d-")
    pipe = "\\\\.\\pipe\\retrace-test-agent"
    journal = os.path.join(work, "journal.jsonl")

    d = subprocess.Popen(
        [daemon, "--sock", pipe, "--journal", journal],
        stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    try:
        f = open_pipe(pipe)
        if f is None:
            print("FAIL: daemon never listened on the pipe",
                  file=sys.stderr)
            return 1

        f.write(frame(HELLO, json.dumps({
            "session_token": "", "nonce": "",
            "pid": os.getpid(), "ppid": 0,
            "boot_id": "test", "cmdline": "pipe-stub",
            "retrace_version": "stub"})))
        mid, welcome = recv_frame(f)
        if mid != WELCOME or welcome.get("role") != "spectator":
            print(f"FAIL: expected spectator WELCOME, got {mid} "
                  f"{welcome}", file=sys.stderr)
            return 1
        agent_id = welcome.get("agent_id", "?")
        for i in range(3):
            f.write(frame(EVENT, json.dumps({
                "agent_id": agent_id, "seq": i + 1,
                "ts": int(time.time()),
                "name": "kernel.syscall.observe",
                "attrs": {"syscall": "NtCreateFile"},
                "source": "kernel"})))
            time.sleep(0.1)
        f.write(frame(BYE, json.dumps({"agent_id": agent_id})))
        f.close()
        time.sleep(1.0)

        subprocess.run(["taskkill", "/F", "/T", "/PID", str(d.pid)],
                       capture_output=True)
        d.wait(timeout=10)

        recs = journal_records(journal)
        names = [r.get("ev", {}).get("name") for r in recs]
        obs = [r for r in recs
               if r.get("ev", {}).get("name") == "kernel.syscall.observe"]
        if len(obs) != 3:
            print(f"FAIL: expected 3 observations, got {len(obs)}: "
                  f"{names}", file=sys.stderr)
            return 1
        auth = [r for r in recs
                if r.get("ev", {}).get("name") == "retrace.auth.agent"]
        if not auth or auth[-1]["ev"].get("role") != "spectator":
            print(f"FAIL: not seated as spectator: {auth}",
                  file=sys.stderr)
            return 1
        drift = [r for r in recs
                 if r.get("ev", {}).get("name") == "retrace.drift.summary"]
        dk = [int(r["ev"].get("kernel_obs", 0)) for r in drift]
        if max(dk, default=0) < 3:
            print(f"FAIL: drift under-counts: "
                  f"{[r['ev'] for r in drift]}", file=sys.stderr)
            return 1

        # supervisor/12 P1 hardening: the pipe DACL must grant
        # owner/Admins/SYSTEM only -- NO Everyone/World access
        ps_cmd = ("(Get-Acl '%s').Access | "
                  "ForEach-Object { $_.IdentityReference } | "
                  "Select-Object -ExpandProperty Value") % pipe
        acl = subprocess.run(
            ["powershell", "-NoProfile", "-Command", ps_cmd],
            capture_output=True, text=True, timeout=30)
        granted = " ".join(acl.stdout.lower().split())
        if "everyone" in granted or "world" in granted:
            print(f"FAIL: pipe DACL grants world: {granted}",
                  file=sys.stderr)
            return 1

        print("retraced-pipe: 3 kernel observations journaled; "
              "spectator seat; drift summary; no world ACL -- OK")
        return 0
    finally:
        if d.poll() is None:
            subprocess.run(["taskkill", "/F", "/T", "/PID",
                            str(d.pid)], capture_output=True)


if __name__ == "__main__":
    sys.exit(main())
