#!/usr/bin/env python3
"""
rpipe -- the shared harness for the pipe-family integration
tests.

One copy of the machinery every named-pipe test needs: frame
build/parse against the RTRD header, a readiness wait that
knows the canonical pipe prefix, journal reading, and a
capture-output runner. The tests import this module (the
script's directory rides sys.path) -- the folk copies these
replaced drifted in deadlines, path prefixes (the
doubled-backslash class), and header parsing (the version-
field parse bug), each fixed one file at a time.
"""
import json
import os
import struct
import subprocess
import sys
import time

MAGIC = b"RTRD"
PIPE_PREFIX = "\\\\.\\pipe\\"


def frame(mid, payload):
    b = payload.encode()
    return MAGIC + struct.pack("<HHI", 1, mid, len(b)) + b


def recv_exact(f, n):
    buf = b""
    while len(buf) < n:
        chunk = f.read(n - len(buf))
        if not chunk:
            raise EOFError
        buf += chunk
    return buf


def recv_frame(f):
    hdr = recv_exact(f, 12)
    _, _, mid, ln = struct.unpack("<4sHHI", hdr)
    body = recv_exact(f, ln) if ln else b""
    return mid, json.loads(body) if body else {}


def is_pipe_path(path):
    return path.startswith(PIPE_PREFIX)


def open_pipe(path, deadline=10.0):
    """The CONNECTION once the pipe accepts one (raw, unbuffered,
    full duplex) -- None if the deadline passes first."""
    end = time.time() + deadline
    while time.time() < end:
        try:
            return open(path, "r+b", buffering=0)
        except OSError:
            time.sleep(0.1)
    return None


def wait_pipe(path, deadline=10.0):
    """Readiness only: one throwaway open (the daemon re-arms)."""
    end = time.time() + deadline
    while time.time() < end:
        try:
            open(path, "r+b", buffering=0).close()
            return True
        except OSError:
            time.sleep(0.1)
    return False


def wait_sock(path, deadline=5.0):
    """Dual-mode readiness: pipe paths probe with an open,
    socket paths with existence (the POSIX legs)."""
    end = time.time() + deadline
    while time.time() < end:
        if is_pipe_path(path):
            if wait_pipe(path, 0.5):
                return True
        elif os.path.exists(path):
            return True
        time.sleep(0.1)
    return False


def journal_records(path):
    recs = []
    with open(path, errors="replace") as f:
        for ln in f.read().splitlines():
            try:
                recs.append(json.loads(ln))
            except json.JSONDecodeError:
                pass
    return recs


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True,
                          timeout=60, **kw)


def taskkill(pid):
    """Windows force-kill of a test daemon tree (the finally
    safety; happy paths stop the daemon gracefully)."""
    if os.name != "nt":
        return
    subprocess.run(["taskkill", "/F", "/T", "/PID", str(pid)],
                   capture_output=True)
