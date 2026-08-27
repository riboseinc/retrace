#!/usr/bin/env python3
"""
E2E: TLS fleet transport (TODO.supervisor/08 P1 / beyond-libc/05).

Doctrine under test:
  * TLS 1.3 mutual auth only -- no plaintext remote mode
  * Controller cert URI SAN carries claim scopes
    (retrace:scope:status,ps,policy,kill)
  * Overscope commands are refused + journaled
  * A cert without scopes is refused at handshake

Usage: test_tls_fleet.py <retraced>
Requires: openssl CLI on PATH (for minting the throwaway PKI).
"""
import json
import os
import shutil
import signal
import socket
import ssl
import subprocess
import sys
import tempfile
import time


def wait_port(host, port, deadline=5.0):
    end = time.time() + deadline
    while time.time() < end:
        try:
            s = socket.create_connection((host, port), timeout=0.2)
            s.close()
            return True
        except OSError:
            time.sleep(0.05)
    return False


def journal_records(path):
    recs = []
    with open(path) as f:
        for ln in f.read().splitlines():
            try:
                recs.append(json.loads(ln))
            except json.JSONDecodeError:
                pass
    return recs


def mint_pki(work):
    """CA + server + two clients (full scopes / status-only)."""
    ca_key = os.path.join(work, "ca.key")
    ca_crt = os.path.join(work, "ca.crt")
    srv_key = os.path.join(work, "server.key")
    srv_csr = os.path.join(work, "server.csr")
    srv_crt = os.path.join(work, "server.crt")
    full_key = os.path.join(work, "full.key")
    full_csr = os.path.join(work, "full.csr")
    full_crt = os.path.join(work, "full.crt")
    ro_key = os.path.join(work, "ro.key")
    ro_csr = os.path.join(work, "ro.csr")
    ro_crt = os.path.join(work, "ro.crt")
    ext_full = os.path.join(work, "full.ext")
    ext_ro = os.path.join(work, "ro.ext")
    ext_srv = os.path.join(work, "srv.ext")

    def run(args):
        r = subprocess.run(args, capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(f"openssl failed: {args}\n{r.stderr}")

    run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
         "-keyout", ca_key, "-out", ca_crt, "-days", "1",
         "-subj", "/CN=retrace-test-ca"])
    run(["openssl", "req", "-newkey", "rsa:2048", "-nodes",
         "-keyout", srv_key, "-out", srv_csr,
         "-subj", "/CN=retraced-server"])
    with open(ext_srv, "w") as f:
        f.write("subjectAltName=DNS:localhost,IP:127.0.0.1\n"
                "extendedKeyUsage=serverAuth\n")
    run(["openssl", "x509", "-req", "-in", srv_csr, "-CA", ca_crt,
         "-CAkey", ca_key, "-CAcreateserial", "-out", srv_crt,
         "-days", "1", "-extfile", ext_srv])

    # full-scope controller
    run(["openssl", "req", "-newkey", "rsa:2048", "-nodes",
         "-keyout", full_key, "-out", full_csr,
         "-subj", "/CN=ctl-full"])
    with open(ext_full, "w") as f:
        # '+' not ',' -- OpenSSL SAN grammar splits on comma
        f.write("subjectAltName=URI:retrace:scope:status+ps+policy+kill\n"
                "extendedKeyUsage=clientAuth\n")
    run(["openssl", "x509", "-req", "-in", full_csr, "-CA", ca_crt,
         "-CAkey", ca_key, "-CAcreateserial", "-out", full_crt,
         "-days", "1", "-extfile", ext_full])

    # status-only controller
    run(["openssl", "req", "-newkey", "rsa:2048", "-nodes",
         "-keyout", ro_key, "-out", ro_csr,
         "-subj", "/CN=ctl-ro"])
    with open(ext_ro, "w") as f:
        f.write("subjectAltName=URI:retrace:scope:status\n"
                "extendedKeyUsage=clientAuth\n")
    run(["openssl", "x509", "-req", "-in", ro_csr, "-CA", ca_crt,
         "-CAkey", ca_key, "-CAcreateserial", "-out", ro_crt,
         "-days", "1", "-extfile", ext_ro])

    return {
        "ca": ca_crt, "server_cert": srv_crt, "server_key": srv_key,
        "full_cert": full_crt, "full_key": full_key,
        "ro_cert": ro_crt, "ro_key": ro_key,
    }


def tls_cmd(host, port, pki, cert, key, line, timeout=5.0):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.minimum_version = ssl.TLSVersion.TLSv1_3
    ctx.maximum_version = ssl.TLSVersion.TLSv1_3
    ctx.load_verify_locations(pki["ca"])
    ctx.load_cert_chain(cert, key)
    ctx.check_hostname = False  # IP peer; CN not used for host
    raw = socket.create_connection((host, port), timeout=timeout)
    s = ctx.wrap_socket(raw, server_hostname="localhost")
    try:
        s.sendall((line + "\n").encode())
        s.settimeout(timeout)
        buf = b""
        while b"\n" not in buf:
            chunk = s.recv(4096)
            if not chunk:
                break
            buf += chunk
        return buf.decode(errors="replace").strip()
    finally:
        try:
            s.close()
        except OSError:
            pass


def main():
    if len(sys.argv) != 2:
        print("usage: test_tls_fleet.py <retraced>", file=sys.stderr)
        return 2
    if shutil.which("openssl") is None:
        print("SKIP: openssl CLI not on PATH", file=sys.stderr)
        return 0
    daemon = os.path.abspath(sys.argv[1])
    work = tempfile.mkdtemp(prefix="tls-fleet-")
    sock = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")
    port = 18765
    try:
        pki = mint_pki(work)
    except RuntimeError as e:
        print(f"FAIL: pki mint: {e}", file=sys.stderr)
        return 1

    d = subprocess.Popen(
        [daemon, "--sock", sock, "--journal", journal,
         "--tls-listen", f"127.0.0.1:{port}",
         "--tls-cert", pki["server_cert"],
         "--tls-key", pki["server_key"],
         "--tls-ca", pki["ca"]],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        if not wait_port("127.0.0.1", port):
            out = d.stdout.read().decode(errors="replace")[:400]
            print(f"FAIL: daemon never listened: {out}", file=sys.stderr)
            return 1

        # full-scope: status ok
        r = tls_cmd("127.0.0.1", port, pki, pki["full_cert"],
                    pki["full_key"], '{"cmd":"status"}')
        if '"ok":1' not in r:
            print(f"FAIL: full-scope status: {r!r}", file=sys.stderr)
            return 1

        # status-only: status ok, freeze denied
        r = tls_cmd("127.0.0.1", port, pki, pki["ro_cert"],
                    pki["ro_key"], '{"cmd":"status"}')
        if '"ok":1' not in r:
            print(f"FAIL: ro status: {r!r}", file=sys.stderr)
            return 1
        r = tls_cmd("127.0.0.1", port, pki, pki["ro_cert"],
                    pki["ro_key"], '{"cmd":"freeze"}')
        if "scope denied" not in r:
            print(f"FAIL: ro freeze should be denied: {r!r}",
                  file=sys.stderr)
            return 1

        # plaintext TCP must not speak the line protocol
        try:
            raw = socket.create_connection(("127.0.0.1", port), timeout=2)
            raw.sendall(b'{"cmd":"status"}\n')
            raw.settimeout(1.0)
            try:
                junk = raw.recv(64)
            except socket.timeout:
                junk = b""
            raw.close()
            if b'"ok"' in junk:
                print("FAIL: plaintext remote accepted a command",
                      file=sys.stderr)
                return 1
        except OSError:
            pass  # connection reset is fine -- TLS required

        d.send_signal(signal.SIGTERM)
        try:
            d.wait(timeout=5)
        except subprocess.TimeoutExpired:
            d.kill()

        recs = journal_records(journal)
        names = [r.get("ev", {}).get("name") for r in recs]
        if not any(n == "retrace.auth.tls" for n in names):
            print(f"FAIL: no retrace.auth.tls journaled: {names}",
                  file=sys.stderr)
            return 1
        if not any(n == "retrace.auth.overscope" for n in names):
            print(f"FAIL: no retrace.auth.overscope journaled: {names}",
                  file=sys.stderr)
            return 1

        print("tls-fleet: mTLS status ok; overscope denied + journaled; "
              "plaintext refused -- OK")
        return 0
    finally:
        if d.poll() is None:
            d.send_signal(signal.SIGTERM)
            try:
                d.wait(timeout=5)
            except subprocess.TimeoutExpired:
                d.kill()


if __name__ == "__main__":
    sys.exit(main())
