#!/usr/bin/env python3
"""
E2E: the SUPERVISED loop on Windows (TODO.supervisor/12 P0): a real
target under retrace.dll whose in-process agent joins the named-pipe
daemon, and policy reaches it. The whole chain in one test:
retraced (pipes) + retrace-win-run (inject) + the agent thread
(HELLO with the env nonce -> full peer) + POLICY_SET push via the
ctl pipe + POLICY_ACK + BYE in the journal.

Usage: test_supervised_win.py <retraced> <retrace-win-run>
"""
import json
import os
import subprocess
import sys
import tempfile
import time

PIPE_AGENT = "\\\\.\\pipe\\retrace-sup-agent"
PIPE_CTL = "\\\\.\\pipe\\retrace-sup-ctl"
NONCE = "a1b2c3d4e5f60718293a4b5c6d7e8f90"


def journal_records(path):
    recs = []
    with open(path, errors="replace") as f:
        for ln in f.read().splitlines():
            try:
                recs.append(json.loads(ln))
            except json.JSONDecodeError:
                pass
    return recs


def wait_pipe(name, deadline=10.0):
    end = time.time() + deadline
    while time.time() < end:
        try:
            f = open(name, "r+b", buffering=0)
            return f
        except OSError:
            time.sleep(0.1)
    return None


def main():
    if len(sys.argv) != 3:
        print("usage: test_supervised_win.py <retraced>"
              " <retrace-win-run>", file=sys.stderr)
        return 2
    if os.name != "nt":
        print("SKIP: supervised loop exists only on Windows",
              file=sys.stderr)
        return 0
    daemon, win_run = (os.path.abspath(p) for p in sys.argv[1:3])
    # the DLL rides the v2 tree; MSVC multi-config nests it one
    # level deeper (Release/) than Ninja, and the launcher may
    # sit in either -- walk up until it is found
    dll = None
    root = os.path.dirname(win_run)
    for _ in range(5):
        for sub in ("src/v2", "src/backends/preload_msvc", "."):
            for cfg in ("", "Release/", "Debug/"):
                cand = os.path.join(root, sub, cfg, "retrace.dll")
                if os.path.exists(cand):
                    dll = cand
                    break
            if dll is not None:
                break
        if dll is not None:
            break
        root = os.path.dirname(root)
    if dll is None:
        print("FAIL: retrace.dll not found near retrace-win-run",
              file=sys.stderr)
        return 1

    work = tempfile.mkdtemp(prefix="sup-win-")
    journal = os.path.join(work, "journal.jsonl")

    d = subprocess.Popen(
        [daemon, "--sock", PIPE_AGENT, "--ctl", PIPE_CTL,
         "--journal", journal, "--nonce", NONCE],
        stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    try:
        ctl = wait_pipe(PIPE_CTL)
        if ctl is None:
            print("FAIL: daemon ctl pipe never appeared",
                  file=sys.stderr)
            return 1

        # the target: a process with a real lifetime -- the agent
        # thread needs the target alive to connect, HELLO, and
        # drain (cmd /c exit dies before the connect finishes)
        env = dict(os.environ)
        env.update({
            "RETRACE_SUPERVISOR": "1",
            "RETRACE_SUPERVISOR_SOCK": PIPE_AGENT,
            "RETRACE_SUPERVISOR_NONCE": NONCE,
            # cmd/ping are native Win32 -- no CRT calls, so the
            # ucrt-level hooks never dispatch and the lazy agent
            # kick never fires. The ntdll layer (the static-CRT
            # smoke's pattern) sees their CreateFileW.
            "RETRACE_WIN_NTDLL": "1",
        })
        r = subprocess.run(
            [win_run, "--lib", dll, "cmd.exe", "/c", "ping", "-n", "4",
             "127.0.0.1"],
            env=env, capture_output=True, text=True, timeout=60)
        # the target's own exit code is what matters; a win-run
        # usage failure is a harness problem
        if r.returncode not in (0, 1):
            print(f"FAIL: win-run rc={r.returncode}: "
                  f"{r.stderr[:300]}", file=sys.stderr)
            return 1
        time.sleep(2.0)  # HELLO + heartbeat settle

        # policy push through the ctl pipe (full-scope local peer)
        policy = json.dumps({
            "cmd": "policy_push",
            "blob": json.dumps({
                "policy": {"epoch": 7},
                "intercept_scripts": [],
            }),
        }) + "\n"
        ctl.write(policy.encode())
        ctl.flush()
        time.sleep(2.0)
        ctl.close()

        subprocess.run(["taskkill", "/F", "/T", "/PID", str(d.pid)],
                       capture_output=True)
        d.wait(timeout=10)

        recs = journal_records(journal)
        names = [r.get("ev", {}).get("name") for r in recs]
        auth = [r for r in recs
                if r.get("ev", {}).get("name") == "retrace.auth.agent"]
        if not auth:
            print(f"FAIL: no agent auth record: {names}",
                  file=sys.stderr)
            return 1
        # nonceless would be 'spectator'; the env carried the nonce
        if auth[-1]["ev"].get("role") != "full":
            print(f"FAIL: agent not a full peer: {auth}",
                  file=sys.stderr)
            return 1
        if not any(n and n.startswith("retrace.policy") for n in names):
            print(f"FAIL: no policy records: {names}", file=sys.stderr)
            return 1

        print("supervised-win: target joined as full peer; policy "
              "pushed; journal carries the loop -- OK")
        return 0
    finally:
        if ctl is not None:
            try:
                ctl.close()
            except OSError:
                pass
        if d.poll() is None:
            subprocess.run(["taskkill", "/F", "/T", "/PID",
                            str(d.pid)], capture_output=True)


if __name__ == "__main__":
    sys.exit(main())
