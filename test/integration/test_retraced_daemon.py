#!/usr/bin/env python3
"""
E2E smoke for the retraced daemon P0 (TODO.supervisor/02).

A stub agent (the conformance suite's framing, one socket):
HELLO -> WELCOME, HEARTBEAT, EVENTs, POLICY_ACK, PING->PONG,
BYE; then daemon shutdown. Asserts:

  1. WELCOME carries a minted agent_id and heartbeat_ms
  2. every EVENT landed in the journal, chain intact
  3. reboot replays the journal (crash-only state)
  4. TAMPERING with a journal line makes the next boot refuse
     to start (fail-closed authority, the audit story)

Usage: test_retraced_daemon.py <retraced-binary>
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


def frame(mid, payload: dict) -> bytes:
    body = json.dumps(payload, separators=(",", ":")).encode()
    return MAGIC + struct.pack("<HHI", 1, mid, len(body)) + body


def recv_frame(sock):
    hdr = b""
    while len(hdr) < 12:
        chunk = sock.recv(12 - len(hdr))
        if not chunk:
            return None, None
        hdr += chunk
    _magic, ver, mid, ln = struct.unpack("<4sHHI", hdr)
    body = b""
    while len(body) < ln:
        body += sock.recv(ln - len(body))
    return mid, json.loads(body)


def wait_socket(path, deadline=10.0):
    end = time.time() + deadline
    while time.time() < end:
        if os.path.exists(path):
            try:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                s.connect(path)
                return s
            except OSError:
                pass
        time.sleep(0.1)
    return None


def start_daemon(binpath, sock, journal):
    p = subprocess.Popen(
        [binpath, "--sock", sock, "--journal", journal],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return p


def main():
    if len(sys.argv) != 2:
        print("usage: test_retraced_daemon.py <retraced>",
              file=sys.stderr)
        return 2
    binary = os.path.abspath(sys.argv[1])
    if not os.path.exists(binary):
        print(f"FAIL: daemon not found: {binary}", file=sys.stderr)
        return 2

    work = tempfile.mkdtemp(prefix="retraced-smoke-")
    sock = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")

    # -- boot 1: full happy path --------------------------------
    d = start_daemon(binary, sock, journal)
    s = wait_socket(sock)
    if s is None:
        d.kill()
        print("FAIL: daemon never listened", file=sys.stderr)
        return 1

    s.sendall(frame(1, {  # HELLO
        "session_token": "sess-1", "pid": os.getpid(),
        "ppid": 1, "boot_id": "b1", "cmdline": "./stub",
        "retrace_version": "2.38.0"}))
    mid, welcome = recv_frame(s)
    if mid != 16 or "agent_id" not in welcome or \
            welcome.get("heartbeat_ms", 0) <= 0:
        print(f"FAIL: bad WELCOME: {mid} {welcome}", file=sys.stderr)
        d.kill()
        return 1
    agent_id = welcome["agent_id"]

    s.sendall(frame(2, {"agent_id": agent_id, "seq": 1}))  # HEARTBEAT
    s.sendall(frame(4, {"agent_id": agent_id, "seq": 2,
                        "ts": 1, "name": "retrace.jail.denied",
                        "attrs": {"retrace.jail.path": "/etc/shadow"}}))
    s.sendall(frame(4, {"agent_id": agent_id, "seq": 3,
                        "ts": 2, "name": "retrace.drift.hit",
                        "attrs": {}}))
    s.sendall(frame(3, {"agent_id": agent_id,
                        "policy_epoch": 7}))  # POLICY_ACK
    s.sendall(frame(19, {}))                    # PING
    mid, pong = recv_frame(s)
    if mid != 19:
        print(f"FAIL: PING not answered: {mid}", file=sys.stderr)
        d.kill()
        return 1
    s.sendall(frame(6, {"agent_id": agent_id}))  # BYE
    time.sleep(0.3)

    d.send_signal(signal.SIGTERM)
    try:
        out1 = d.communicate(timeout=5)[0]
    except subprocess.TimeoutExpired:
        d.kill()
        out1 = d.communicate()[0]

    # assertions on boot 1
    if "agent %s" % agent_id not in out1 and agent_id not in out1:
        print(f"FAIL: registration not logged: {out1[:300]!r}",
              file=sys.stderr)
        return 1
    with open(journal) as f:
        lines = [ln for ln in f.read().splitlines() if ln.strip()]
    try:
        events = [json.loads(ln) for ln in lines
                  if "retrace." in ln]
    except json.JSONDecodeError as e:
        bad = [ln for ln in lines if "retrace." in ln][0] \
            if any("retrace." in ln for ln in lines) else "?"
        print(f"FAIL: journal line unparseable: {e}\n  {bad[:200]}",
              file=sys.stderr)
        return 1
    names = [e.get("ev", {}).get("name") for e in events]
    # this test HELLOs nonceless: the auth event records the
    # spectator role (TODO.supervisor/08) and is journaled at
    # HELLO -- BEFORE the agent's own events
    if len(events) != 3 or \
            "retrace.jail.denied" not in names or \
            names[0] != "retrace.auth.agent":
        print(f"FAIL: journal events wrong: {lines}", file=sys.stderr)
        return 1
    # daemon-authored records (retrace.auth.agent, session
    # minting) are the exception to per-agent attribution
    if any(json.loads(ln)["agent"] not in (agent_id, "daemon")
           for ln in lines):
        print("FAIL: journal attribution wrong", file=sys.stderr)
        return 1
    print(f"boot 1 ok: agent={agent_id}, {len(lines)} journal lines")

    # -- boot 2: replay ----------------------------------------
    d2 = start_daemon(binary, sock, journal)
    s2 = wait_socket(sock)
    if s2 is None:
        d2.kill()
        print("FAIL: reboot never listened", file=sys.stderr)
        return 1
    time.sleep(0.3)
    d2.send_signal(signal.SIGTERM)
    try:
        out2 = d2.communicate(timeout=5)[0]
    except subprocess.TimeoutExpired:
        d2.kill()
        out2 = d2.communicate()[0]
    if "journal replayed" not in out2 or "chain ok" not in out2:
        print(f"FAIL: replay not reported: {out2[:400]!r}",
              file=sys.stderr)
        return 1
    print("boot 2 ok: journal replayed, chain verified")

    # -- boot 3: tampered journal -> refuse to start -------------
    with open(journal) as f:
        content = f.read().splitlines()
    content[0] = content[0].replace('"seq"', '"sq"')  # tamper
    with open(journal, "w") as f:
        f.write("\n".join(content) + "\n")
    d3 = start_daemon(binary, sock, journal)
    try:
        out3, _ = d3.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        d3.kill()
        out3 = d3.communicate()[0]
    if d3.returncode == 0 or "chain broken" not in out3:
        print(f"FAIL: tampered journal accepted: rc="
              f"{d3.returncode} out={out3[:300]!r}", file=sys.stderr)
        return 1
    print("boot 3 ok: tampered journal refused (fail-closed)")

    print("PASS: daemon P0 (register/heartbeat/event/ack/ping/"
          "bye + replay + tamper detection)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
