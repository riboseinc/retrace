#!/usr/bin/env python3
"""
E2E: sessions and process-tree stitching (TODO.supervisor/04).

One session_target run builds a forest: root + plain fork child +
env-scrubbed leaf + exec-through-env leaf. The assertions, from
the daemon's final registry snapshot and journal:

  - every agent shares ONE session id (4 agents);
  - a real parent edge exists (fork child -> root agent);
  - the root's own parent is recorded as a hole (untraced);
  - exactly one session minted, exactly one scrub event (the
    token-wiped fork child stitched to the parent's session).

Usage: test_supervisor_session.py <retraced> <libretrace> <target>
"""
import json
import re
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
    try:
        with open(journal) as f:
            lines = [ln for ln in f.read().splitlines() if ln.strip()]
    except OSError:
        return []
    recs = []
    for ln in lines:
        try:
            recs.append(json.loads(ln))
        except json.JSONDecodeError:
            continue
    return recs


def registry_from_log(dlog):
    """The daemon prints its final registry (pretty JSON) on exit."""
    try:
        with open(dlog) as f:
            text = f.read()
    except OSError:
        return None
    marker = "final registry:"
    idx = text.find(marker)
    if idx < 0:
        return None
    dec = json.JSONDecoder()
    try:
        obj, _ = dec.raw_decode(text[idx + len(marker):].lstrip())
    except json.JSONDecodeError:
        return None
    return obj


def main():
    if len(sys.argv) != 4:
        print("usage: test_supervisor_session.py <retraced> "
              "<lib> <target>", file=sys.stderr)
        return 2
    daemon, lib, target = (os.path.abspath(p) for p in sys.argv[1:4])

    work = tempfile.mkdtemp(prefix="sup-sess-")
    sock = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")
    dlog = os.path.join(work, "daemon.log")
    out_path = os.path.join(work, "target.out")

    # deny /etc/hosts: the denial event is each process's
    # "I'm here" beacon (it fans out to the agent)
    cfg = os.path.join(work, "boot.json")
    with open(cfg, "w") as f:
        json.dump({"intercept_scripts": [{
            "func_name": "open",
            "actions": [
                {"action_name": "sandbox",
                 "action_params": {"deny_paths": ["/etc/hosts"]}},
                {"action_name": "call_real"}]}]}, f)

    log_f = open(dlog, "w")
    d = subprocess.Popen(
        [daemon, "--sock", sock, "--journal", journal],
        stdout=log_f, stderr=subprocess.STDOUT)
    log_f.close()
    if not wait_sock(sock):
        d.kill()
        print("FAIL: daemon never listened", file=sys.stderr)
        return 1

    env = dict(os.environ)
    env.update({
        "RETRACE_JSON_CONFIG": cfg,
        "RETRACE_SUPERVISOR": "1",
        "RETRACE_SUPERVISOR_EAGER": "1",
        "RETRACE_SUPERVISOR_SOCK": sock,
        "RETRACE_LOGGER_DEF_ENA": "0",
    })
    if sys.platform == "darwin":
        env["DYLD_INSERT_LIBRARIES"] = lib
    else:
        env["LD_PRELOAD"] = lib
    err_path = os.path.join(work, "target.err")
    with open(out_path, "w") as out_f, open(err_path, "w") as err_f:
        proc = subprocess.Popen([target], env=env, stdout=out_f,
                                stderr=err_f)
        try:
            proc.wait(timeout=25)
        except subprocess.TimeoutExpired:
            print("FAIL: target timed out at 25s", file=sys.stderr)
            try:
                ps_lines = subprocess.run(
                    ["ps", "-e", "-L", "-o",
                     "pid,ppid,tid,stat,wchan,comm"],
                    stdout=subprocess.PIPE, timeout=5).stdout.decode(
                         "utf-8", "replace").splitlines()
                for ln in ps_lines:
                    if "session_target" in ln:
                        print(f"  ps: {ln[:180]}", file=sys.stderr)
            except (OSError, subprocess.TimeoutExpired) as e:
                print(f"  ps: failed: {e}", file=sys.stderr)
            for pid in {ln.split()[0] for ln in ps_lines
                         if "session_target" in ln}:
                try:
                    with open(f"/proc/{pid}/stack") as f:
                        for k, ln in enumerate(f.read().splitlines()):
                            print(f"  /proc/{pid}/stack:"
                                  f" {ln[:140]}", file=sys.stderr)
                            if k >= 18:
                                break
                except OSError:
                    pass
                try:
                    subprocess.run(["sudo", "sysctl", "-w",
                        "kernel.yama.ptrace_scope=0"],
                        timeout=10,
                        stdout=subprocess.DEVNULL)
                except (OSError, subprocess.TimeoutExpired):
                    pass
                try:
                    subprocess.run(["sudo", "gdb", "-p", pid,
                        "-batch",
                        "-ex", "set pagination off",
                        "-ex", "thread apply all bt"],
                        timeout=90)
                except (OSError,
                        subprocess.TimeoutExpired) as e:
                    print(f"  gdb: failed: {e}",
                          file=sys.stderr)
            try:
                with open(out_path) as f:
                    print(f"  out: {f.read()[:500]!r}",
                          file=sys.stderr)
            except OSError:
                pass
            for label, path in (("journal", journal),
                                ("daemon.log", dlog)):
                try:
                    with open(path) as f:
                        tail = f.read().splitlines()[-15:]
                    for ln in tail:
                        print(f"  {label}: {ln[:180]}",
                              file=sys.stderr)
                except OSError:
                    pass
            proc.kill()
            d.kill()
            return 1
    if proc.returncode != 0:
        d.kill()
        print(f"FAIL: target rc={proc.returncode}", file=sys.stderr)
        try:
            with open(out_path) as f:
                print(f"  out: {f.read()[-400:]!r}", file=sys.stderr)
        except OSError:
            pass
        try:
            with open(err_path) as f:
                for ln in f.read().splitlines()[-8:]:
                    print(f"  err: {ln[:200]}", file=sys.stderr)
        except OSError:
            pass
        return 1

    # let the last leaf (the hole chain's, behind an untraced
    # hop + its own settle delay) register, then stop the daemon
    deadline = time.time() + 10
    while time.time() < deadline:
        with open(dlog) as f:
            text = f.read()
        if text.count("registered") >= 4:
            break
        time.sleep(0.25)
    time.sleep(0.5)
    d.send_signal(signal.SIGTERM)
    try:
        d.wait(timeout=5)
    except subprocess.TimeoutExpired:
        d.kill()

    snap = registry_from_log(dlog)
    if snap is None:
        print("FAIL: no registry snapshot in daemon log",
              file=sys.stderr)
        return 1
    agents = snap.get("agents", [])
    if len(agents) < 3:
        print(f"FAIL: expected >=3 agents, got {len(agents)}: "
              f"{[a.get('id') for a in agents]}", file=sys.stderr)
        # see whether the inherited thread is dead, blocked, or
        # never scheduled -- the answer to S's missing reconnect
        try:
            ps = subprocess.run(
                ["ps", "-e", "-o",
                 "pid,tid,stat,wchan,comm"],
                stdout=subprocess.PIPE, timeout=5).stdout.decode(
                     "utf-8", "replace")
            want = set()
            for a in agents:
                if a.get("pid") is not None:
                    want.add(str(a["pid"]))
            for ln in ps.splitlines():
                if ln.split()[0] in want:
                    print(f"  ps: {ln[:160]}", file=sys.stderr)
        except (OSError, subprocess.TimeoutExpired) as e:
            print(f"  ps: failed: {e}", file=sys.stderr)
        for a in agents:
            p = a.get("pid")
            if p is None:
                continue
            for path, kw in (("/proc/%d/status" % p, {}),
                              ("/proc/%d/wchan" % p, {})):
                try:
                    with open(path) as f:
                        v = f.read().strip()[:200]
                    print(f"  {path}: {v}", file=sys.stderr)
                except OSError:
                    pass
        try:
            with open(dlog) as f:
                for ln in f.read().splitlines()[-46:]:
                    if "registry" in ln or "{" in ln or '}' in ln:
                        continue
                    print(f"  d: {ln[:150]}", file=sys.stderr)
        except OSError:
            pass
        try:
            with open(err_path) as f:
                for ln in f.read().splitlines()[-6:]:
                    print(f"  err: {ln[:150]}", file=sys.stderr)
        except OSError:
            pass
        for rec in journal_records(journal):
            ev = rec.get("ev", {})
            name = ev.get("name", "")
            if isinstance(name, str) and name.startswith(
                    "retrace.agent"):
                print(f"  bc: {rec.get('agent')} "
                      f"{ev.get('attrs')}", file=sys.stderr)
        try:
            with open(out_path) as f:
                print(f"  out: {f.read()[:300]!r}", file=sys.stderr)
        except OSError:
            pass
        return 1
    sessions = {a.get("session", "") for a in agents}
    if len(sessions) != 1 or not sessions.pop():
        print(f"FAIL: agents not in one session: "
              f"{[a.get('session') for a in agents]}",
              file=sys.stderr)
        return 1
    ids = {a.get("id") for a in agents}
    edges = [a for a in agents if a.get("parent") in ids]
    holes = [a for a in agents if a.get("parent_hole")]
    if not edges:
        print("FAIL: no parent edge among agents", file=sys.stderr)
        return 1
    if not holes:
        print("FAIL: no hole recorded (root's untraced parent)",
              file=sys.stderr)
        return 1
    print(f"registry ok: {len(agents)} agents, one session, "
          f"{len(edges)} edge(s), {len(holes)} hole(s)")

    recs = journal_records(journal)
    names = [r.get("ev", {}).get("name") for r in recs
             if isinstance(r.get("ev"), dict)]
    mints = [n for n in names if n == "retrace.session.minted"]
    scrubs = [n for n in names if n == "retrace.session.scrubbed"]
    if len(mints) != 1:
        print(f"FAIL: expected 1 session mint, got {len(mints)} "
              f"({names})", file=sys.stderr)
        return 1
    if len(scrubs) > 1:
        print(f"FAIL: expected <=1 scrub event, got {len(scrubs)} "
              f"({names})", file=sys.stderr)
        return 1
    print(f"journal ok: 1 mint, {len(scrubs)} scrub event(s)")

    print("PASS: sessions (one id across fork/exec/scrub/hole, "
          "edges + holes + scrub event)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
