#!/usr/bin/env python3
"""
E2E: pyretrace, the Python runtime agent (TODO.beyond-libc/04).

The runtime agent joins the SAME supervisor session the preload
agents use and attributes runtime-internal semantics: a file
read by a python `open()` arrives as py.file.read with the path;
a socket() arrives as py.socket.create -- the layer a libc
interposer cannot name. The daemon journals them as ordinary
events of the same session.

Usage: test_pyretrace.py <retraced> <pyretrace-module-dir>
"""
import json
import os
import signal
import subprocess
import sys
import tempfile
import time


def wait_sock(path, deadline=5.0):
    end = time.time() + deadline
    while time.time() < end:
        if path.startswith(r"\\.\pipe\\"):
            try:
                open(path, "r+b", buffering=0).close()
                return True
            except OSError:
                pass
        elif os.path.exists(path):
            return True
        time.sleep(0.1)
    return False


def journal_records(path):
    recs = []
    with open(path) as f:
        for ln in f.read().splitlines():
            try:
                recs.append(json.loads(ln))
            except json.JSONDecodeError:
                pass
    return recs


CHILD = r"""
import sys, time
sys.path.insert(0, {moddir!r})
import pyretrace
if not pyretrace.supervise():
    print("NO-OP (env absent) is wrong here", file=sys.stderr)
    sys.exit(3)
with open({sentinel!r}) as f:
    f.read()
try:
    import socket as _s
    _s.socket().close()
except OSError:
    pass
time.sleep(1.5)
pyretrace.emit("py.test.marker", kind="direct")
time.sleep(1.0)
pyretrace.stop()
"""


def main():
    if len(sys.argv) != 3:
        print("usage: test_pyretrace.py <retraced> <module-dir>",
              file=sys.stderr)
        return 2
    daemon, moddir = (os.path.abspath(p) for p in sys.argv[1:3])

    work = tempfile.mkdtemp(prefix="pyrt-")
    sock = (r"\\.\pipe\\retrace-py-e2e" if os.name == "nt"
            else os.path.join(work, "agent.sock"))
    journal = os.path.join(work, "journal.jsonl")
    nonce_file = os.path.join(work, "nonce.txt")
    sentinel = os.path.join(work, "sentinel.txt")
    with open(sentinel, "w") as f:
        f.write("py\n")

    d = subprocess.Popen(
        [daemon, "--sock", sock, "--journal", journal,
         "--nonce-file", nonce_file],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        if not wait_sock(sock):
            print("FAIL: daemon never listened", file=sys.stderr)
            return 1
        with open(nonce_file) as f:
            nonce = f.read().strip()

        env = dict(os.environ)
        env.update({
            "RETRACE_SUPERVISOR": "1",
            "RETRACE_SUPERVISOR_SOCK": sock,
            "RETRACE_SUPERVISOR_NONCE": nonce,
        })
        child = subprocess.run(
            [sys.executable, "-c",
             CHILD.format(moddir=moddir, sentinel=sentinel)],
            env=env, capture_output=True, timeout=30)
        if child.returncode != 0:
            print(f"FAIL: child rc={child.returncode}: "
                  f"{child.stderr.decode()[:300]}", file=sys.stderr)
            return 1

        # durability: stop the daemon gracefully before reading
        d.send_signal(signal.SIGTERM)
        try:
            d.wait(timeout=5)
        except subprocess.TimeoutExpired:
            d.kill()

        recs = journal_records(journal)
        names = [r.get("ev", {}).get("name") for r in recs]
        for want, label in (
                ("py.file.read", "the runtime-attributed file read"),
                ("py.socket.create", "the socket creation"),
                ("py.test.marker", "the direct emit")):
            if want not in names:
                print(f"FAIL: {label} not journaled: {names}",
                      file=sys.stderr)
                return 1
        # the auth record shows the runtime agent as a full peer
        auth = [r for r in recs if r.get("ev", {}).get("name") ==
                "retrace.auth.agent"]
        if not auth or auth[-1]["ev"].get("role") != "full":
            print(f"FAIL: runtime agent not full role: {auth}",
                  file=sys.stderr)
            return 1

        print("pyretrace: file read + socket + direct emit "
              "journaled; runtime agent a full peer -- OK")
        return 0
    finally:
        if d.poll() is None:
            d.send_signal(signal.SIGTERM)
            try:
                d.wait(timeout=5)
            except subprocess.TimeoutExpired:
                d.kill()


if __name__ == "__main__":
    sys.exit(main())
