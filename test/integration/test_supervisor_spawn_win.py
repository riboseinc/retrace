#!/usr/bin/env python3
"""
E2E: the audited launch plane on Windows (TODO.impl/11) -- the
POSIX spawn E2E's twin. The daemon's spawn seam drives the
win-run machinery (CreateProcess suspended + DLL injection,
the nonce riding the environment block), the workload joins
through the agent pipe, and every departure lands in the
journal via the handle sweep (SIGCHLD's Windows analogue).

Flow: spawn -> join (status shows the agent) -> the workload
exits with a known code -> the journal carries
retrace.ctl.spawn AND retrace.ctl.exit with that code; then a
second run exercises kill -> exit.

Usage: test_supervisor_spawn_win.py <retraced> <retrace.dll> <spawn-target>
"""
import json
import os
import subprocess
import sys
import tempfile
import time

PIPE_AGENT = "\\\\.\\pipe\\retrace-spw-agent"
PIPE_CTL = "\\\\.\\pipe\\retrace-spw-ctl"
NONCE = "b1b2c3d4e5f60718293a4b5c6d7e8f90"


def wait_pipe(name, timeout=10.0):
    import ctypes

    k32 = ctypes.windll.kernel32
    GENERIC_READ_WRITE = 0xC0000000
    OPEN_EXISTING = 3
    deadline = time.time() + timeout
    while time.time() < deadline:
        h = k32.CreateFileA(name.encode(), GENERIC_READ_WRITE,
                            0, None, OPEN_EXISTING, 0, None)
        if h != -1:
            return h
        time.sleep(0.1)
    return None


def pipe_roundtrip(h, line):
    import ctypes

    k32 = ctypes.windll.kernel32
    data = (line + "\n").encode()
    written = ctypes.c_ulong(0)
    if not k32.WriteFile(h, data, len(data),
                         ctypes.byref(written), None):
        return None
    buf = ctypes.create_string_buffer(4096)
    got = ctypes.c_ulong(0)
    out = b""
    deadline = time.time() + 10
    while time.time() < deadline:
        if not k32.ReadFile(h, buf, 4096,
                            ctypes.byref(got), None):
            break
        out += buf.raw[:got.value]
        if b"\n" in out:
            return out.decode(errors="replace").strip()
    return out.decode(errors="replace").strip() or None


def journal_records(path):
    recs = []
    if not os.path.exists(path):
        return recs
    with open(path, "r", errors="replace") as f:
        for ln in f:
            ln = ln.strip()
            if not ln:
                continue
            try:
                recs.append(json.loads(ln))
            except json.JSONDecodeError:
                pass
    return recs


def ctl_verbs(name, timeout=10.0):
    """yield (send_line, read_reply) bound to the ctl pipe"""
    h = wait_pipe(name, timeout)
    if h is None:
        return None
    return h


def diag(label, h, journal, pid=None, dlog=None):
    """failure forensics: what the journal holds (raw lines --
    a malformed line the parser dropped is visible), whether
    the daemon still answers, and whether the pid still lives"""
    names = [r.get("ev", {}).get("name")
             for r in journal_records(journal)]
    print(f"DIAG {label}: records={names}", file=sys.stderr)
    if os.path.exists(journal):
        with open(journal, "r", errors="replace") as f:
            lines = [ln.strip() for ln in f if ln.strip()]
        for ln in lines[:15]:
            print(f"DIAG {label}: raw {ln[:200]}", file=sys.stderr)
    if pid is not None:
        r = subprocess.run(
            ["tasklist", "/FI", f"PID eq {pid}"],
            capture_output=True, text=True)
        alive = str(pid) in (r.stdout or "")
        print(f"DIAG {label}: pid {pid} alive={alive}",
              file=sys.stderr)
    if h is not None:
        st = pipe_roundtrip(h, json.dumps({"cmd": "status"}))
        print(f"DIAG {label}: status={st}", file=sys.stderr)
    if dlog is not None:
        try:
            with open(dlog, "r", errors="replace") as f:
                tail = f.readlines()[-15:]
            for ln in tail:
                print(f"DIAG {label}: daemon {ln.rstrip()}",
                      file=sys.stderr)
        except OSError:
            pass


def main():
    if len(sys.argv) != 4:
        print("usage: test_supervisor_spawn_win.py <retraced> "
              "<retrace.dll> <spawn-target>", file=sys.stderr)
        return 2
    if os.name != "nt":
        print("SKIP: the audited launch plane is Windows-only")
        return 0

    daemon, dll, target = (os.path.abspath(p) for p in sys.argv[1:4])
    work = tempfile.mkdtemp(prefix="spw-")
    journal = os.path.join(work, "journal.jsonl")

    dlog = os.path.join(work, "daemon.log")
    dlog_h = open(dlog, "w")
    d = subprocess.Popen(
        [daemon, "--sock", PIPE_AGENT, "--ctl", PIPE_CTL,
         "--journal", journal, "--nonce", NONCE],
        stdout=dlog_h, stderr=subprocess.STDOUT)
    try:
        h = ctl_verbs(PIPE_CTL)
        if h is None:
            print("FAIL: daemon ctl pipe never appeared",
                  file=sys.stderr)
            return 1
        import ctypes

        k32 = ctypes.windll.kernel32

        # 1. spawn: the audited launch (dll injected, nonce in
        #    the environment block). The marker dir rides argv:
        #    the target heartbeats RAW Win32 (no dispatch).
        markers = os.path.join(work, "marks")
        os.makedirs(markers, exist_ok=True)
        spawn = json.dumps({
            "cmd": "spawn",
            "argv": [target, markers],
            "preload": dll,
        })
        reply = pipe_roundtrip(h, spawn)
        if reply is None or '"ok":1' not in reply:
            print(f"FAIL: spawn reply: {reply}", file=sys.stderr)
            return 1
        pid = json.loads(reply).get("pid", 0)
        print(f"spawn: pid {pid}")

        # 2. join: the agent HELLOs through the pipe (EAGER)
        joined = False
        deadline = time.time() + 15
        while time.time() < deadline:
            st = pipe_roundtrip(h, json.dumps({"cmd": "status"}))
            if st and '"agents":1' in st.replace(" ", ""):
                joined = True
                break
            time.sleep(0.5)
        if not joined:
            print("FAIL: the workload never joined (agents != 1)",
                  file=sys.stderr)
            return 1
        print("join: agent visible in status")

        # 3. the departure: the target exits with a known code
        exited = None
        deadline = time.time() + 25
        while time.time() < deadline:
            for r in journal_records(journal):
                ev = r.get("ev", {})
                if ev.get("name") == "retrace.ctl.exit" and \
                        int(ev.get("pid", 0)) == pid:
                    exited = ev
                    break
            if exited is not None:
                break
            time.sleep(0.5)
        if exited is None:
            print("FAIL: no retrace.ctl.exit for the pid",
                  file=sys.stderr)
            marks = sorted(os.listdir(markers)) if \
                os.path.isdir(markers) else []
            print(f"DIAG exit-missing: markers={marks}",
                  file=sys.stderr)
            diag("exit-missing", h, journal, pid, dlog)
            return 1
        if exited.get("code") != 7:
            print(f"FAIL: exit code {exited.get('code')} != 7",
                  file=sys.stderr)
            return 1
        print("exit: journaled with the workload's code")

        # 4. the kill arm: spawn, kill, exit record
        reply = pipe_roundtrip(h, spawn)
        pid2 = json.loads(reply).get("pid", 0) if reply else 0
        if pid2 <= 0:
            print(f"FAIL: second spawn reply: {reply}",
                  file=sys.stderr)
            return 1
        kr = pipe_roundtrip(h, json.dumps(
            {"cmd": "kill", "pid": pid2}))
        if kr is None or '"ok":1' not in kr:
            print(f"FAIL: kill reply: {kr}", file=sys.stderr)
            return 1
        killed = None
        deadline = time.time() + 15
        while time.time() < deadline:
            for r in journal_records(journal):
                ev = r.get("ev", {})
                if ev.get("name") == "retrace.ctl.exit" and \
                        int(ev.get("pid", 0)) == pid2:
                    killed = ev
                    break
            if killed is not None:
                break
            time.sleep(0.5)
        if killed is None:
            print("FAIL: no exit record after kill",
                  file=sys.stderr)
            return 1
        print("kill: departure journaled")

        k32.CloseHandle(h)
        subprocess.run(
            ["taskkill", "/F", "/T", "/PID", str(d.pid)],
            capture_output=True)
        d.wait(timeout=10)

        # the launch records: every spawn is journaled
        names = [r.get("ev", {}).get("name")
                 for r in journal_records(journal)]
        if names.count("retrace.ctl.spawn") != 2:
            print(f"FAIL: {names.count('retrace.ctl.spawn')} "
                  f"spawn records (want 2)", file=sys.stderr)
            return 1
        print("journal: 2 launches + 2 departures")

        print("PASS: the audited launch plane holds on Windows")
        return 0
    finally:
        subprocess.run(
            ["taskkill", "/F", "/T", "/PID", str(d.pid)],
            capture_output=True)


if __name__ == "__main__":
    sys.exit(main())
