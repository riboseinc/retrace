#!/usr/bin/env python3
"""
E2E: policy distribution (TODO.supervisor/05).

One target runs the whole time while the daemon is restarted
under three successive policies -- live tightening, freeze,
and a replay attempt:

  phase 1 (tighten): policy epoch 1 denies one path. The
    target starts with a permissive config; after the push the
    denied probe flips to rc=-1 while allowed stays open.

  phase 2 (freeze): policy epoch 2 is a wildcard freeze. The
    allowed probe flips to rc=-1 too and -- the kernel truth --
    the creatable probe stops leaving files behind (the real
    open(O_CREAT) never runs again).

  phase 3 (replay): the daemon returns with epoch 1 -- an old
    epoch replayed at a live agent. The agent refuses it; the
    journal shows applied=false with the held epoch.

Usage: test_supervisor_policy.py <retraced> <libretrace> <target>
"""
import json
import os
import signal
import subprocess
import sys
import tempfile
import time

DENIED = "/etc/hosts"
ALLOWED = "/etc/protocols"


def wait_sock(path, deadline=5.0):
    end = time.time() + deadline
    while time.time() < end:
        if os.path.exists(path):
            return True
        time.sleep(0.1)
    return False


def start_daemon(daemon, sock, journal, policy_file):
    d = subprocess.Popen(
        [daemon, "--sock", sock, "--journal", journal,
         "--policy", policy_file],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not wait_sock(sock):
        d.kill()
        return None
    return d


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


def acks(recs):
    out = []
    for rec in recs:
        ev = rec.get("ev", {})
        if "policy_epoch" in ev and "applied" in ev:
            out.append(ev)
    return out


def last_status(out_path):
    """Parse the target's latest iter= line; None until one."""
    try:
        with open(out_path) as f:
            lines = [ln for ln in f.read().splitlines()
                     if ln.startswith("iter=")]
    except OSError:
        return None
    if not lines:
        return None
    st = {}
    for part in lines[-1].split():
        k, _, v = part.partition("=")
        st[k] = v
    return st


def wait_status(out_path, predicate, deadline):
    """Wait until the target's latest line satisfies predicate."""
    end = time.time() + deadline
    while time.time() < end:
        st = last_status(out_path)
        if st is not None and predicate(st):
            return st
        time.sleep(0.2)
    return None


def wait_quiescence(dirpath, need, stable_s, deadline):
    """Freeze signal: >= need files existed, then no new ones for
    stable_s (pre-freeze pace is one file/second, so a stable
    window longer than that is unambiguous)."""
    end = time.time() + deadline
    last_count = -1
    last_change = time.time()
    while time.time() < end:
        n = len(os.listdir(dirpath))
        if n != last_count:
            last_count = n
            last_change = time.time()
        if n >= need and (time.time() - last_change) >= stable_s:
            return n
        time.sleep(0.25)
    return None


def write_policy(path, epoch, scripts, expires=0):
    pol = {"epoch": epoch}
    if expires:
        pol["expires"] = expires
    with open(path, "w") as f:
        json.dump({"policy": pol, "intercept_scripts": scripts}, f)


def main():
    if len(sys.argv) != 4:
        print("usage: test_supervisor_policy.py <retraced> "
              "<lib> <target>", file=sys.stderr)
        return 2
    daemon, lib, target = (os.path.abspath(p) for p in sys.argv[1:4])

    work = tempfile.mkdtemp(prefix="sup-pol-")
    sock = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")
    out_path = os.path.join(work, "target.out")
    creatable_dir = tempfile.mkdtemp(prefix="sup-pol-files-")

    # a permissive boot config: opens pass through untouched
    boot_cfg = os.path.join(work, "boot.json")
    with open(boot_cfg, "w") as f:
        json.dump({"intercept_scripts": [
            {"func_name": "open",
             "actions": [{"action_name": "call_real"}]}]}, f)

    # policy 1: deny DENIED only
    pol1 = os.path.join(work, "policy1.json")
    write_policy(pol1, 1, [{
        "func_name": "open",
        "actions": [
            {"action_name": "sandbox",
             "action_params": {"deny_paths": [DENIED]}},
            {"action_name": "call_real"}]}])

    # policy 2: freeze everything
    pol2 = os.path.join(work, "policy2.json")
    write_policy(pol2, 2, [{
        "func_name": "*",
        "actions": [{"action_name": "freeze"}]}])

    # policy 3: replay of epoch 1 (already superseded)
    pol3 = os.path.join(work, "policy3.json")
    write_policy(pol3, 1, [{
        "func_name": "open",
        "actions": [
            {"action_name": "sandbox",
             "action_params": {"deny_paths": [DENIED]}},
            {"action_name": "call_real"}]}])

    # ---- phase 1: live tightening -----------------------------
    # daemon first: the eager agent connects on its first try
    # (the reconnect path is exercised by the later restarts)
    d1 = start_daemon(daemon, sock, journal, pol1)
    if d1 is None:
        print("FAIL: daemon never listened (phase 1)", file=sys.stderr)
        return 1

    # ---- the target: eager agent, permissive boot config -----
    out_f = open(out_path, "w")
    env = dict(os.environ)
    env.update({
        "RETRACE_JSON_CONFIG": boot_cfg,
        "RETRACE_SUPERVISOR": "1",
        "RETRACE_SUPERVISOR_EAGER": "1",
        "RETRACE_SUPERVISOR_SOCK": sock,
        "RETRACE_LOGGER_DEF_ENA": "0",
    })
    if sys.platform == "darwin":
        env["DYLD_INSERT_LIBRARIES"] = lib
    else:
        env["LD_PRELOAD"] = lib
    proc = subprocess.Popen(
        [target, "25", DENIED, ALLOWED, creatable_dir],
        env=env, stdout=out_f, stderr=subprocess.DEVNULL)

    st = wait_status(out_path,
                     lambda s: s.get("denied") == "-1", 10)
    if st is None:
        stop_daemon(d1, sock)
        proc.kill()
        print("FAIL: denial never landed live", file=sys.stderr)
        print(f"  last: {last_status(out_path)}", file=sys.stderr)
        return 1
    print(f"phase 1 ok: tightened live at iter {st.get('iter')} "
          f"(denied={st['denied']}, allowed={st['allowed']})")

    # ---- phase 2: freeze across a daemon restart --------------
    # under a wildcard freeze the target's own printf/time are
    # frozen too -- stdout goes silent BY DESIGN. the observable
    # is the kernel truth: the real open(O_CREAT) never runs
    # again, so the creatable directory goes quiet.
    stop_daemon(d1, sock)
    d2 = start_daemon(daemon, sock, journal, pol2)
    if d2 is None:
        proc.kill()
        print("FAIL: daemon never listened (phase 2)", file=sys.stderr)
        return 1
    nfiles = wait_quiescence(creatable_dir, need=2, stable_s=3.5,
                             deadline=15)
    if nfiles is None:
        stop_daemon(d2, sock)
        proc.kill()
        print("FAIL: freeze never landed (files kept appearing)",
              file=sys.stderr)
        print(f"  last stdout: {last_status(out_path)}",
              file=sys.stderr)
        print(f"  files: {sorted(os.listdir(creatable_dir))}",
              file=sys.stderr)
        return 1
    st = last_status(out_path)
    if st is None or int(st.get("creatable", -1)) < 0:
        stop_daemon(d2, sock)
        proc.kill()
        print(f"FAIL: pre-freeze line wrong: {st}", file=sys.stderr)
        return 1
    files = set(os.listdir(creatable_dir))
    time.sleep(1.5)
    if set(os.listdir(creatable_dir)) != files:
        stop_daemon(d2, sock)
        proc.kill()
        print("FAIL: frozen target created files after quiescence",
              file=sys.stderr)
        return 1
    print(f"phase 2 ok: frozen with {nfiles} files on disk; zero "
          f"real opens since (stdout silent, target alive)")

    # ---- phase 3: epoch replay refused ------------------------
    stop_daemon(d2, sock)
    d3 = start_daemon(daemon, sock, journal, pol3)
    if d3 is None:
        proc.kill()
        print("FAIL: daemon never listened (phase 3)", file=sys.stderr)
        return 1
    replay_ack = None
    end = time.time() + 10
    while time.time() < end:
        recs = acks(journal_records(journal))
        for a in recs:
            if a.get("applied") is False:
                replay_ack = a
                break
        if replay_ack is not None:
            break
        time.sleep(0.3)
    stop_daemon(d3, sock)
    proc.kill()

    if replay_ack is None:
        print("FAIL: no refusal ACK for the replayed epoch",
              file=sys.stderr)
        return 1
    if replay_ack.get("policy_epoch") != 2:
        print(f"FAIL: refusal kept wrong epoch: {replay_ack}",
              file=sys.stderr)
        return 1
    print(f"phase 3 ok: replay refused ({replay_ack.get('reason')}), "
          f"epoch held at 2")

    applied = [a for a in acks(journal_records(journal))
               if a.get("applied") is True]
    epochs = sorted({a.get("policy_epoch") for a in applied})
    if epochs != [1, 2]:
        print(f"FAIL: applied epochs wrong: {epochs}", file=sys.stderr)
        return 1
    print(f"journal ok: applied epochs {epochs} + 1 refusal")

    print("PASS: policy (live tighten + freeze + kernel truth + "
          "replay refusal)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
