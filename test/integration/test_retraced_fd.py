#!/usr/bin/env python3
"""
E2E: retraced socket activation + privilege drop
(TODO.supervisor/08 P2 / TODO.beyond-libc/05 P2).

--fd N: the daemon serves an already-bound, listening agent
socket handed to it (socket activation; systemd's LISTEN_FDS
convention maps to the same path). The daemon must NOT unlink a
socket it did not create.

--user/--group: after every socket is bound the daemon drops
privileges; a requested drop that fails exits 2 (fail-closed).
Root-only section, self-skips otherwise.

Usage: test_retraced_fd.py <retraced>
"""
import json
import os
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time

MAGIC = b"RTRD"
HELLO, HEARTBEAT, EVENT, BYE = 1, 2, 4, 6


def frame(mid, payload):
    b = payload.encode()
    return MAGIC + struct.pack("<HHI", 1, mid, len(b)) + b


def stub_agent(sock_path, nonce=""):
    """HELLO + one event + BYE; returns the WELCOME role."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock_path)
    s.sendall(frame(HELLO, json.dumps({
        "session_token": "", "nonce": nonce,
        "pid": os.getpid(), "ppid": os.getppid(),
        "boot_id": "test", "cmdline": "stub",
        "retrace_version": "stub"})))
    hdr = b""
    while len(hdr) < 12:
        hdr += s.recv(12 - len(hdr))
    _, _, mid, ln = struct.unpack("<4sHHI", hdr)
    body = b""
    while len(body) < ln:
        body += s.recv(ln - len(body))
    if mid != 16:
        raise RuntimeError("no WELCOME")
    welcome = json.loads(body)
    s.sendall(frame(EVENT, json.dumps({
        "agent_id": welcome.get("agent_id", "?"), "seq": 1,
        "ts": int(time.time()), "name": "fd.test.event",
        "attrs": {}, "source": "libc"})))
    s.sendall(frame(BYE, json.dumps(
        {"agent_id": welcome.get("agent_id", "?")})))
    s.close()
    return welcome.get("role", "?")


def wait_file(path, deadline=5.0):
    end = time.time() + deadline
    while time.time() < end:
        if os.path.exists(path):
            return True
        time.sleep(0.1)
    return False


def wait_output(path, needle, deadline=5.0):
    end = time.time() + deadline
    while time.time() < end:
        try:
            with open(path, errors="replace") as f:
                if needle in f.read():
                    return True
        except OSError:
            pass
        time.sleep(0.1)
    return False


def read_file(path):
    try:
        with open(path, errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def pick_unprivileged_user():
    """An existing non-root account (images differ: Alpine CI has
    no 'nobody'); None when none exists."""
    try:
        with open("/etc/passwd") as f:
            for ln in f:
                parts = ln.strip().split(":")
                if len(parts) >= 3 and parts[0] and \
                        parts[2].isdigit() and 0 < int(parts[2]) < 65534:
                    return parts[0]
    except OSError:
        pass
    return None


def main():
    if len(sys.argv) != 2:
        print("usage: test_retraced_fd.py <retraced>", file=sys.stderr)
        return 2
    daemon = os.path.abspath(sys.argv[1])

    work = tempfile.mkdtemp(prefix="fd-act-")
    sock = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")
    d_out = os.path.join(work, "d.out")

    # 1. socket activation: WE bind + listen, hand the fd over.
    # pass_fds preserves the fd NUMBER; the child must be told
    # the same number.
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    listener.bind(sock)
    listener.listen(8)
    os.chmod(sock, 0o660)
    fdnum = listener.fileno()

    with open(d_out, "w") as df:
        d = subprocess.Popen(
            [daemon, "--sock", sock, "--journal", journal,
             "--fd", str(fdnum)],
            stdin=subprocess.DEVNULL, stdout=df,
            stderr=subprocess.STDOUT,
            pass_fds=(fdnum,))
    try:
        if not wait_output(d_out, "inherited on fd"):
            print(f"FAIL: daemon never adopted the fd: "
                  f"{read_file(d_out)[:300]}", file=sys.stderr)
            return 1
        # the round trip itself proves the daemon serves OUR
        # listener (this process never accept()s)
        role = None
        end = time.time() + 5
        while time.time() < end:
            try:
                role = stub_agent(sock)
                break
            except OSError:
                time.sleep(0.1)
        if role not in ("spectator", "full"):
            print(f"FAIL: unexpected role {role}", file=sys.stderr)
            return 1
        d.send_signal(signal.SIGTERM)
        try:
            d.wait(timeout=5)
        except subprocess.TimeoutExpired:
            d.kill()

        # the daemon must NOT unlink a socket it inherited
        if not os.path.exists(sock):
            print("FAIL: daemon unlinked the inherited socket",
                  file=sys.stderr)
            return 1
        with open(journal) as f:
            recs = [json.loads(ln) for ln in f if ln.strip()]
        if not any(r.get("ev", {}).get("name") == "fd.test.event"
                   for r in recs):
            print("FAIL: event not journaled via inherited socket",
                  file=sys.stderr)
            return 1
        listener.close()

        # 2. privilege drop: root-only, self-skips
        if os.geteuid() != 0:
            print("fd-activation: inherited socket served; no "
                  "unlink; drop section skipped (not root) -- OK")
            return 0
        drop_user = pick_unprivileged_user()
        if drop_user is None:
            print("fd-activation: inherited socket served; no "
                  "unlink; drop section skipped (no unprivileged "
                  "account) -- OK")
            return 0

        d2_out = os.path.join(work, "d2.out")
        with open(d2_out, "w") as df:
            d2 = subprocess.Popen(
                [daemon, "--sock", os.path.join(work, "a2.sock"),
                 "--journal", os.path.join(work, "j2.jsonl"),
                 "--user", drop_user],
                stdin=subprocess.DEVNULL, stdout=df,
                stderr=subprocess.STDOUT)
        try:
            if not wait_output(d2_out, "running as uid="):
                print(f"FAIL: no drop line: "
                      f"{read_file(d2_out)[:300]}", file=sys.stderr)
                return 1
        finally:
            d2.send_signal(signal.SIGTERM)
            try:
                d2.wait(timeout=5)
            except subprocess.TimeoutExpired:
                d2.kill()
        # a bad user must fail-closed (exit 2), not run elevated
        d3 = subprocess.run(
            [daemon, "--sock", os.path.join(work, "a3.sock"),
             "--journal", os.path.join(work, "j3.jsonl"),
             "--user", "no-such-user-xyz"],
            capture_output=True, text=True, timeout=10)
        if d3.returncode != 2:
            print(f"FAIL: unknown user must exit 2, got "
                  f"{d3.returncode}: {d3.stdout[:200]}",
                  file=sys.stderr)
            return 1
        print("fd-activation: inherited socket served; no unlink; "
              "drop to nobody ok; unknown user fail-closed -- OK")
        return 0
    finally:
        if d.poll() is None:
            d.send_signal(signal.SIGTERM)
            try:
                d.wait(timeout=5)
            except subprocess.TimeoutExpired:
                d.kill()
        if os.path.exists(sock):
            os.unlink(sock)


if __name__ == "__main__":
    sys.exit(main())
