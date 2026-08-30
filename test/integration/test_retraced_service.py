#!/usr/bin/env python3
"""
E2E: retraced as a Windows SERVICE (TODO.supervisor/12 P1).

The SCM lifecycle on a real (elevated) Windows runner: sc create
with the daemon binary + flags in binPath, sc start, a pipe client
connects while the service runs, sc stop (the SCM stop maps to the
daemon's own g_stop -> graceful loop exit), sc delete. The
--exit-after guard from v2.55.0 rides along as the watchdog belt.

Usage: test_retraced_service.py <retraced>
"""
import json
import os
import struct
import subprocess
import sys
import tempfile
import time

MAGIC = b"RTRD"
HELLO, BYE, WELCOME = 1, 6, 16
SVC = "retrace-svc-test"


def frame(mid, payload):
    b = payload.encode()
    return MAGIC + struct.pack("<HHI", 1, mid, len(b)) + b


def recv_exact(f, n):
    buf = b""
    while len(buf) < n:
        c = f.read(n - len(buf))
        if not c:
            raise EOFError
        buf += c
    return buf


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True,
                          timeout=60, **kw)


def main():
    if len(sys.argv) != 2:
        print("usage: test_retraced_service.py <retraced>",
              file=sys.stderr)
        return 2
    if os.name != "nt":
        print("SKIP: SCM exists only on Windows", file=sys.stderr)
        return 0
    daemon = os.path.abspath(sys.argv[1])

    work = tempfile.mkdtemp(prefix="svc-")
    pipe = "\\\\.\\pipe\\retrace-svc-agent"
    journal = os.path.join(work, "journal.jsonl")

    run(["sc", "stop", SVC])
    run(["sc", "delete", SVC])

    r = run(["sc", "create", SVC,
             "binPath=", f'"{daemon}" --sock {pipe} --journal "{journal}" --exit-after 300',
             "start=", "auto"])
    if r.returncode != 0:
        print(f"FAIL: sc create: {r.stderr[:300]}", file=sys.stderr)
        return 1
    try:
        r = run(["sc", "start", SVC])
        if r.returncode != 0:
            print(f"FAIL: sc start: {r.stderr[:300]}", file=sys.stderr)
            return 1

        # wait RUNNING + pipe up
        up = None
        for _ in range(50):
            q = run(["sc", "query", SVC])
            if "RUNNING" in q.stdout:
                try:
                    up = open(pipe, "r+b", buffering=0)
                    break
                except OSError:
                    pass
            time.sleep(0.2)
        if up is None:
            q = run(["sc", "query", SVC])
            print(f"FAIL: service never served the pipe; "
                  f"sc query rc={q.returncode} "
                  f"out={q.stdout[:200]!r} "
                  f"err={q.stderr[:200]!r}", file=sys.stderr)
            return 1

        up.write(frame(HELLO, json.dumps({
            "session_token": "", "nonce": "",
            "pid": os.getpid(), "ppid": 0, "boot_id": "svc",
            "cmdline": "svc-stub", "retrace_version": "stub"})))
        hdr = recv_exact(up, 12)
        mid, ln = struct.unpack("<HH", hdr[4:8])[0], \
            struct.unpack("<I", hdr[8:12])[0]
        if mid != WELCOME:
            print(f"FAIL: no WELCOME ({mid})", file=sys.stderr)
            return 1
        up.write(frame(BYE, "{}"))
        up.close()

        r = run(["sc", "stop", SVC])
        if r.returncode != 0:
            print(f"FAIL: sc stop: {r.stderr[:300]}", file=sys.stderr)
            return 1
        time.sleep(1.0)

        if os.path.exists(journal):
            with open(journal, errors="replace") as f:
                recs = [json.loads(ln) for ln in f if ln.strip()]
            names = [x.get("ev", {}).get("name") for x in recs]
            if "retrace.auth.agent" not in names:
                print(f"FAIL: service agent not journaled: {names}",
                      file=sys.stderr)
                return 1
        else:
            print("FAIL: service wrote no journal", file=sys.stderr)
            return 1

        print("service: create -> start -> pipe client -> stop -> "
              "journal -- OK")
        return 0
    finally:
        run(["sc", "stop", SVC])
        run(["sc", "delete", SVC])


if __name__ == "__main__":
    sys.exit(main())
