#!/usr/bin/env python3
"""
E2E: retrace-ctl spawn -- the doctrine-shaped launch (the
threat model's "host process control").

The daemon forks the workload armed to join ITSELF: supervisor
env, agent socket, the nonce ("handed to spawners"), EAGER
connect, and the preload the caller chose. One command must
produce the whole chain: the journaled launch, the agent's
HELLO (full role -- the nonce presented, never a spectator
seat), visibility in ps, and a clean kill through the same
control plane.

Usage: test_supervisor_spawn.py <retraced> <retrace-ctl> <lib> <target>
"""
import json
import os
import signal
import subprocess
import sys
import tempfile
import time

TEST_NONCE = "0123456789abcdef0123456789abcdef"


def wait_sock(path, deadline=5.0):
    end = time.time() + deadline
    while time.time() < end:
        if os.path.exists(path):
            return True
        time.sleep(0.1)
    return False


def ctl(ctl_bin, sock, *args):
    p = subprocess.run([ctl_bin, "--sock", sock, *args],
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, timeout=15)
    return p.returncode, p.stdout.decode()


def journal_events(path):
    try:
        with open(path) as f:
            return [json.loads(l)["ev"] for l in f if l.strip()]
    except (OSError, KeyError, ValueError):
        return []


def wait_journal(path, name, pred, deadline=15.0):
    """Wait until an event of `name` satisfying `pred` lands."""
    end = time.time() + deadline
    while time.time() < end:
        for ev in journal_events(path):
            if ev.get("name") == name and pred(ev):
                return ev
        time.sleep(0.4)
    return None


def pid_alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def fail(msg):
    print(f"FAIL: {msg}", file=sys.stderr)
    return 1


def main():
    if len(sys.argv) != 5:
        print("usage: test_supervisor_spawn.py <retraced> "
              "<retrace-ctl> <lib> <target>", file=sys.stderr)
        return 2
    daemon, ctl_bin, lib, target = (
        os.path.abspath(p) for p in sys.argv[1:5])

    work = tempfile.mkdtemp(prefix="sup-spawn-")
    sock = os.path.join(work, "agent.sock")
    ctl_sock = os.path.join(work, "ctl.sock")
    journal = os.path.join(work, "journal.jsonl")

    d = subprocess.Popen(
        [daemon, "--sock", sock, "--journal", journal,
         "--nonce", TEST_NONCE, "--ctl", ctl_sock],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not wait_sock(ctl_sock):
        d.kill()
        return fail("daemon ctl socket never appeared")

    # ---- spawn: one command, the whole armament ---------------
    rc, out = ctl(ctl_bin, ctl_sock, "spawn",
                  "--preload", lib, "--", target, "12")
    if rc != 0:
        stop_daemon(d, ctl_sock)
        return fail(f"spawn rc={rc}: {out!r}")
    reply = json.loads(out)
    if not reply.get("ok") or reply.get("pid", 0) <= 0:
        stop_daemon(d, ctl_sock)
        return fail(f"spawn refused: {reply}")
    pid = reply["pid"]
    print(f"spawn ok: pid {pid}")

    # ---- the launch is audited before anything else ------------
    ev = wait_journal(journal, "retrace.ctl.spawn",
                      lambda e: e.get("pid") == pid, 5.0)
    if ev is None:
        stop_daemon(d, ctl_sock)
        return fail("no retrace.ctl.spawn in the journal")
    print("journal ok: retrace.ctl.spawn audited "
          f"({ev.get('argv0')})")

    # ---- the agent joins: HELLO + full role, never spectator --
    ev = wait_journal(journal, "retrace.auth.agent",
                      lambda e: True, 15.0)
    if ev is None:
        stop_daemon(d, ctl_sock)
        return fail("spawned workload never HELLO'd")
    if ev.get("role") != "full":
        stop_daemon(d, ctl_sock)
        return fail(f"spawned agent got role {ev.get('role')!r} "
                    "-- the nonce did not reach the child")
    print("journal ok: retrace.auth.agent role=full "
          "(nonce presented)")

    # ---- the fleet sees it -------------------------------------
    seen = None
    end = time.time() + 15
    while time.time() < end:
        rc, out = ctl(ctl_bin, ctl_sock, "ps")
        if rc == 0:
            agents = json.loads(out).get("registry", {}).get(
                "agents", [])
            for a in agents:
                if a.get("pid") == pid:
                    seen = a
                    break
        if seen is not None:
            break
        time.sleep(0.4)
    if seen is None:
        stop_daemon(d, ctl_sock)
        return fail("ps never showed the spawned agent")
    if seen.get("spectator"):
        stop_daemon(d, ctl_sock)
        return fail("spawned agent seated as spectator")
    print(f"ps ok: agent live (state {seen.get('state')})")

    # ---- reap through the same control plane -------------------
    rc, out = ctl(ctl_bin, ctl_sock, "kill", str(pid))
    if rc != 0 or not json.loads(out).get("ok"):
        stop_daemon(d, ctl_sock)
        return fail(f"kill rc={rc}: {out!r}")
    end = time.time() + 8
    while pid_alive(pid) and time.time() < end:
        time.sleep(0.3)
    if pid_alive(pid):
        stop_daemon(d, ctl_sock)
        return fail("spawned workload survived the kill")
    ev = wait_journal(journal, "retrace.ctl.kill",
                      lambda e: e.get("pid") == pid, 5.0)
    if ev is None:
        stop_daemon(d, ctl_sock)
        return fail("kill not journaled")
    print("kill ok: reaped and audited")

    stop_daemon(d, ctl_sock)
    print("PASS: spawn joined, visible, audited, reaped")
    return 0


def stop_daemon(d, sock):
    if d is None:
        return
    d.send_signal(signal.SIGTERM)
    try:
        d.wait(timeout=5)
    except subprocess.TimeoutExpired:
        d.kill()
    deadline = time.time() + 2
    while os.path.exists(sock) and time.time() < deadline:
        time.sleep(0.1)


if __name__ == "__main__":
    sys.exit(main())
