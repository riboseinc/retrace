#!/usr/bin/env python3
"""
E2E: control-plane transport auth (TODO.supervisor/08 P0).

The negative suite from the plan, against one daemon:

  - socket mode gates: agent 0660, ctl 0600, nonce file 0600;
  - a HELLO with a WRONG nonce registers as a spectator
    (evidence flows, policy never arrives: pushed=0);
  - a HELLO with NO nonce is a spectator too;
  - the correct nonce (read back from --nonce-file) is a full
    agent (spectator=0) and receives the policy push;
  - a connection from another uid (via sudo -u nobody) is
    refused and journaled -- skipped when sudo/nobody are
    unavailable (macOS local runs have both).

Usage: test_supervisor_auth.py <retraced> <lib> <target>
"""
import json
import os
import socket
import stat
import subprocess
import sys
import tempfile
import time

WRONG_NONCE = "ffffffffffffffffffffffffffffffff"


def wait_sock(path, deadline=5.0):
    end = time.time() + deadline
    while time.time() < end:
        if os.path.exists(path):
            return True
        time.sleep(0.1)
    return False


def ctl_call(ctl_sock, line):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(5)
    s.connect(ctl_sock)
    s.sendall((line + "\n").encode())
    out = b""
    while not out.endswith(b"\n"):
        chunk = s.recv(4096)
        if not chunk:
            break
        out += chunk
    s.close()
    return json.loads(out.decode() or "{}")


def wait_agent(ctl_sock, prev_count, timeout=15):
    """Poll ps until the registry grows past prev_count."""
    end = time.time() + timeout
    while time.time() < end:
        r = ctl_call(ctl_sock, '{"cmd":"ps"}')
        if r.get("ok"):
            agents = r["registry"].get("agents", [])
            if len(agents) > prev_count:
                return agents
        time.sleep(0.5)
    return None


def spawn_target(target, lib, sock, nonce):
    env = dict(os.environ)
    env.update({
        "RETRACE_SUPERVISOR": "1",
        "RETRACE_SUPERVISOR_EAGER": "1",
        "RETRACE_SUPERVISOR_SOCK": sock,
        "RETRACE_LOGGER_DEF_ENA": "0",
    })
    if nonce is not None:
        env["RETRACE_SUPERVISOR_NONCE"] = nonce
    if sys.platform == "darwin":
        env["DYLD_INSERT_LIBRARIES"] = lib
    else:
        env["LD_PRELOAD"] = lib
    return subprocess.Popen([target], env=env,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def journal_records(journal):
    recs = []
    try:
        with open(journal) as f:
            for ln in f.read().splitlines():
                try:
                    recs.append(json.loads(ln))
                except json.JSONDecodeError:
                    pass
    except OSError:
        pass
    return recs


def main():
    if len(sys.argv) != 4:
        print("usage: test_supervisor_auth.py <retraced> "
              "<lib> <target>", file=sys.stderr)
        return 2
    daemon, lib, target = (os.path.abspath(p) for p in sys.argv[1:4])

    work = tempfile.mkdtemp(prefix="sup-auth-")
    sock = os.path.join(work, "agent.sock")
    ctl_sock = os.path.join(work, "ctl.sock")
    journal = os.path.join(work, "journal.jsonl")
    nonce_file = os.path.join(work, "nonce.txt")

    d = subprocess.Popen(
        [daemon, "--sock", sock, "--journal", journal,
         "--ctl", ctl_sock, "--nonce-file", nonce_file],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        if not wait_sock(sock) or not wait_sock(ctl_sock):
            print("FAIL: daemon never listened", file=sys.stderr)
            return 1

        # -- mode gates ----------------------------------------
        for path, want in ((sock, 0o660), (ctl_sock, 0o600),
                           (nonce_file, 0o600)):
            got = stat.S_IMODE(os.stat(path).st_mode)
            if got != want:
                print(f"FAIL: {path} mode {oct(got)} "
                      f"!= {oct(want)}", file=sys.stderr)
                return 1

        with open(nonce_file) as f:
            good_nonce = f.read().strip()
        if len(good_nonce) != 32 or any(
                c not in "0123456789abcdef" for c in good_nonce):
            print(f"FAIL: nonce file not 32 hex: "
                  f"{good_nonce!r}", file=sys.stderr)
            return 1

        # -- wrong nonce => spectator --------------------------
        p1 = spawn_target(target, lib, sock, WRONG_NONCE)
        agents = wait_agent(ctl_sock, 0)
        if agents is None:
            print("FAIL: no registry", file=sys.stderr)
            return 1
        if not agents or agents[-1].get("spectator") != 1:
            print(f"FAIL: wrong-nonce agent not spectator: "
                  f"{agents[-1] if agents else None}",
                  file=sys.stderr)
            return 1
        pol = json.dumps({"policy": {"epoch": 1},
            "intercept_scripts": [
                {"func_name": "*",
                 "actions": [{"action_name": "log_params"}]}]})
        r = ctl_call(ctl_sock,
            json.dumps({"cmd": "policy_push", "blob": pol}))
        if r.get("pushed") != 0:
            print(f"FAIL: policy reached spectators: {r}",
                  file=sys.stderr)
            return 1
        p1.terminate()
        p1.wait(timeout=5)

        # -- no nonce => spectator -----------------------------
        p2 = spawn_target(target, lib, sock, None)
        agents = wait_agent(ctl_sock, 1)
        if not agents or agents[-1].get("spectator") != 1:
            print(f"FAIL: nonceless agent not spectator",
                  file=sys.stderr)
            return 1
        p2.terminate()
        p2.wait(timeout=5)

        # -- correct nonce => full agent -----------------------
        p3 = spawn_target(target, lib, sock, good_nonce)
        agents = wait_agent(ctl_sock, 2)
        full = [a for a in agents if not a.get("spectator")]
        if not full:
            print(f"FAIL: correct-nonce agent never full: "
                  f"{agents}", file=sys.stderr)
            return 1
        # the target is short-lived: push while it is still
        # connected (registration lands before exit; retry)
        pushed = 0
        end = time.time() + 10
        while time.time() < end and pushed < 1:
            r = ctl_call(ctl_sock,
                json.dumps({"cmd": "policy_push", "blob": pol}))
            pushed = r.get("pushed", 0)
            if pushed < 1:
                time.sleep(0.3)
        if pushed < 1:
            print(f"FAIL: full agent missed policy: {r}",
                  file=sys.stderr)
            return 1
        p3.terminate()
        p3.wait(timeout=5)

        # -- journal tells the auth story ----------------------
        recs = journal_records(journal)
        roles = {r.get("ev", {}).get("role")
                 for r in recs
                 if r.get("ev", {}).get("name") ==
                 "retrace.auth.agent"}
        if not {"spectator", "full"} <= roles:
            print(f"FAIL: journal roles {roles}", file=sys.stderr)
            return 1

        # -- wrong uid refused (best effort) -------------------
        # sudo/id may not EXIST at all (Alpine): probe with
        # FileNotFoundError treated as unavailable
        def _have(cmd):
            try:
                return subprocess.run(cmd,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                    ).returncode == 0
            except OSError:
                return False

        have_nobody = _have(["id", "-u", "nobody"])
        sudo = _have(["sudo", "-n", "true"])
        if have_nobody and sudo:
            probe = subprocess.run(
                ["sudo", "-n", "-u", "nobody", sys.executable,
                 "-c",
                 "import socket,sys\n"
                 "s=socket.socket(socket.AF_UNIX,"
                 "socket.SOCK_STREAM)\n"
                 "try:\n"
                 f"    s.connect({sock!r})\n"
                 "except PermissionError:\n"
                 "    sys.exit(1)\n"
                 "s.settimeout(3)\n"
                 "try:\n"
                 "    d=s.recv(16)\n"
                 "    sys.exit(0 if d==b'' else 3)\n"
                 "except socket.timeout:\n"
                 "    sys.exit(2)\n"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL)
            # 0 = connected then closed (the daemon's PEERCRED
            # gate); 1 = EACCES (the 0660 mode gate refused the
            # connect outright). BOTH are refusals; 2 (timeout)
            # and 3 (got data) are not.
            if probe.returncode not in (0, 1):
                print(f"FAIL: wrong-uid peer got through: "
                      f"rc={probe.returncode}", file=sys.stderr)
                return 1
            # the journal record exists when the refusal happened
            # daemon-side (PEERCRED); the mode-gate path never
            # reaches the daemon
            if probe.returncode == 0 and not any(
                    r.get("ev", {}).get("name") ==
                    "retrace.auth.refused"
                    for r in journal_records(journal)):
                print("FAIL: closed peer not journaled",
                      file=sys.stderr)
                return 1

        print("auth: mode gates + nonce roles + peer refusal OK")
        return 0
    finally:
        d.terminate()
        try:
            d.wait(timeout=5)
        except subprocess.TimeoutExpired:
            d.kill()


if __name__ == "__main__":
    sys.exit(main())
