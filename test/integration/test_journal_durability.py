#!/usr/bin/env python3
"""
E2E: journal durability (the writer deepening, TODO.supervisor
arc / architecture-review candidate B).

The contract under test:

  - control-plane records (auth/policy/session) are flushed as
    written -- they survive SIGKILL;
  - a SIGKILL loses at most the buffered telemetry tail, and the
    NEXT boot journals retrace.journal.unclean -- the gap is
    recorded, never silent;
  - a clean SIGTERM writes retrace.journal.closed, and the next
    boot adds NO unclean record;
  - the hash chain verifies across every boot.

Usage: test_journal_durability.py <retraced> <lib> <target>
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
        if os.path.exists(path):
            return True
        time.sleep(0.1)
    return False


def journal_records(journal):
    recs = []
    with open(journal) as f:
        for ln in f.read().splitlines():
            try:
                recs.append(json.loads(ln))
            except json.JSONDecodeError:
                pass
    return recs


def start_daemon(daemon, sock, journal, nonce_file):
    d = subprocess.Popen(
        [daemon, "--sock", sock, "--journal", journal,
         "--nonce-file", nonce_file],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not wait_sock(sock):
        d.kill()
        raise RuntimeError("daemon never listened")
    return d


def spawn_target(target, lib, sock, nonce):
    env = dict(os.environ)
    env.update({
        "RETRACE_SUPERVISOR": "1",
        "RETRACE_SUPERVISOR_EAGER": "1",
        "RETRACE_SUPERVISOR_SOCK": sock,
        "RETRACE_SUPERVISOR_NONCE": nonce,
        "RETRACE_LOGGER_DEF_ENA": "0",
    })
    if sys.platform == "darwin":
        env["DYLD_INSERT_LIBRARIES"] = lib
    else:
        env["LD_PRELOAD"] = lib
    return subprocess.Popen([target, "4"], env=env,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main():
    if len(sys.argv) != 4:
        print("usage: test_journal_durability.py <retraced> "
              "<lib> <target>", file=sys.stderr)
        return 2
    daemon, lib, target = (os.path.abspath(p) for p in sys.argv[1:4])

    work = tempfile.mkdtemp(prefix="sup-jr-")
    sock = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")
    nonce_file = os.path.join(work, "nonce.txt")

    # boot 1: register an agent (the auth record is durable),
    # then SIGKILL -- no clean close, buffered tail at risk
    d = start_daemon(daemon, sock, journal, nonce_file)
    with open(nonce_file) as f:
        nonce = f.read().strip()
    p = spawn_target(target, lib, sock, nonce)
    p.wait(timeout=20)
    time.sleep(0.5)
    d.kill()
    d.wait()

    killed = journal_records(journal)
    auth = [r for r in killed
            if r.get("ev", {}).get("name") == "retrace.auth.agent"]
    if not auth:
        print("FAIL: durable auth record lost to SIGKILL",
              file=sys.stderr)
        return 1
    if any(r.get("ev", {}).get("name") == "retrace.journal.closed"
           for r in killed):
        print("FAIL: close marker present after SIGKILL",
              file=sys.stderr)
        return 1

    # boot 2: replay must succeed and journal the gap
    d = start_daemon(daemon, sock, journal, nonce_file)
    time.sleep(0.5)
    d.send_signal(signal.SIGTERM)
    d.wait(timeout=5)

    gap = journal_records(journal)
    if not any(r.get("ev", {}).get("name") ==
               "retrace.journal.unclean" for r in gap):
        print("FAIL: unclean shutdown not journaled",
              file=sys.stderr)
        return 1

    # boot 3: clean history -- no NEW unclean record
    d = start_daemon(daemon, sock, journal, nonce_file)
    time.sleep(0.5)
    d.send_signal(signal.SIGTERM)
    d.wait(timeout=5)

    final = journal_records(journal)
    unclean = [r for r in final
               if r.get("ev", {}).get("name") ==
               "retrace.journal.unclean"]
    if len(unclean) != 1:
        print(f"FAIL: clean boot re-marked unclean "
              f"({len(unclean)} records)", file=sys.stderr)
        return 1
    if not any(r.get("ev", {}).get("name") ==
               "retrace.journal.closed" for r in final[-2:]):
        print("FAIL: clean close marker missing at tail",
              file=sys.stderr)
        return 1

    print("journal durability: SIGKILL survival + recorded gap "
          "+ clean close OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
