#!/usr/bin/env python3
"""
E2E: journal signing (TODO.impl/07). Hash chains prove local
continuity; the ed25519 seal gives an external auditor the
trust anchor. The daemon seals at graceful close; verify-journal
walks the chain AND checks the seal. Three verdicts: clean
(verified+signed), tampered (chain names its line), wrong key
(signature mismatch).

Usage: test_journal_sign.py <retraced> <retrace-ctl>
"""
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


def main():
    if len(sys.argv) != 3:
        print("usage: test_journal_sign.py <retraced> "
              "<retrace-ctl>", file=sys.stderr)
        return 2
    daemon, ctl_bin = (os.path.abspath(p) for p in sys.argv[1:3])

    # key generation needs the openssl CLI: probe trouble is a
    # skip, never a failure (the noderetrace gate's rule)
    probe = subprocess.run(["openssl", "version"],
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
    if probe.returncode != 0:
        print("SKIP: openssl CLI not available on this leg")
        return 0

    work = tempfile.mkdtemp(prefix="jsign-")
    journal = os.path.join(work, "journal.jsonl")
    key = os.path.join(work, "k.pem")
    pub = os.path.join(work, "pub.pem")
    key2 = os.path.join(work, "k2.pem")
    pub2 = os.path.join(work, "pub2.pem")

    for k, p in ((key, pub), (key2, pub2)):
        subprocess.run(["openssl", "genpkey", "-algorithm", "ed25519",
                        "-out", k], check=True,
                       stdout=subprocess.DEVNULL)
        subprocess.run(["openssl", "pkey", "-in", k, "-pubout",
                        "-out", p], check=True,
                       stdout=subprocess.DEVNULL)

    d = subprocess.Popen(
        [daemon, "--sock", os.path.join(work, "a.sock"),
         "--journal", journal, "--ctl", os.path.join(work, "c.sock"),
         "--nonce-file", os.path.join(work, "n.txt"),
         "--signing-key", key],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not wait_sock(os.path.join(work, "c.sock")):
        d.kill()
        print("FAIL: daemon ctl socket never appeared", file=sys.stderr)
        return 1
    # one event so the journal is non-trivial
    subprocess.run([ctl_bin, "--sock", os.path.join(work, "c.sock"),
                    "status"], stdout=subprocess.DEVNULL)
    time.sleep(0.3)
    d.send_signal(signal.SIGTERM)
    try:
        d.wait(timeout=5)
    except subprocess.TimeoutExpired:
        d.kill()

    with open(journal) as f:
        content = f.read()
    if "retrace.journal.signed" not in content:
        print("FAIL: no seal record in the journal", file=sys.stderr)
        return 1
    print("sealed: journal carries the ed25519 seal")

    def verify(path, pubkey):
        p = subprocess.run([ctl_bin, "verify-journal", path,
                            "--pubkey", pubkey],
                           stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT)
        return p.returncode, p.stdout.decode()

    rc, out = verify(journal, pub)
    if rc != 0 or "verified + signed" not in out:
        print(f"FAIL: clean journal refused: {out!r}", file=sys.stderr)
        return 1
    print(f"clean: {out.strip()}")

    tam = os.path.join(work, "tampered.jsonl")
    with open(tam, "w") as f:
        f.write(content.replace("retrace.journal.closed",
                                "retrace.journal.closed ", 1))
    rc, out = verify(tam, pub)
    if rc == 0 or "chain broken" not in out:
        print(f"FAIL: tampered journal accepted: {out!r}",
              file=sys.stderr)
        return 1
    print(f"tampered: {out.strip()}")

    rc, out = verify(journal, pub2)
    if rc == 0 or "signature mismatch" not in out:
        print(f"FAIL: wrong key accepted: {out!r}", file=sys.stderr)
        return 1
    print("wrong key: signature mismatch")

    print("PASS: sign/verify/tamper/wrong-key all hold")
    return 0


if __name__ == "__main__":
    sys.exit(main())
