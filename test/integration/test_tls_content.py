#!/usr/bin/env python3
"""
E2E: the TLS content lane (TODO.impl/13) -- what was
exfiltrated, not just where.

An HTTPS fetch under the preload with tls_content on SSL_write
and tls_keylog on SSL_CTX_new: the DECRYPTED request line lands
in the daemon journal (redaction transform applied -- the URL
secret arrives starred, never verbatim), and the app's own
OpenSSL logs its keylog lines into RETRACE_TLS_KEYLOG.

Providers without SSL_CTX_set_keylog_callback (LibreSSL) skip
the keylog arm loudly; the content arm is provider-free.

Usage: test_tls_content.py <retraced> <retrace-lib>
"""
import json
import os
import signal
import ssl
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

from rpipe import (wait_sock, journal_records)


class _One(BaseHTTPRequestHandler):
    def do_GET(self):
        body = b"tls lane\n"
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *a):
        pass


CLIENT = r"""
import ssl, sys, urllib.request
ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE
urllib.request.urlopen(sys.argv[1], context=ctx, timeout=10).read()
"""


def main():
    if len(sys.argv) != 3:
        print("usage: test_tls_content.py <retraced> <retrace-lib>",
              file=sys.stderr)
        return 2
    daemon, rtlib = (os.path.abspath(p) for p in sys.argv[1:3])

    work = tempfile.mkdtemp(prefix="tlsct-")
    cert = os.path.join(work, "srv.pem")
    key = os.path.join(work, "srv.key")
    if subprocess.run(["openssl", "req", "-x509", "-newkey",
                       "rsa:2048", "-nodes", "-keyout", key,
                       "-out", cert, "-days", "1", "-subj",
                       "/CN=localhost"],
                      capture_output=True).returncode != 0:
        print("SKIP: openssl CLI unavailable")
        return 0

    srv = HTTPServer(("127.0.0.1", 0), _One)
    port = srv.server_address[1]
    # server-side default context (modern negotiated protocols
    # only; the bare PROTOCOL_TLS_SERVER constant trips the
    # python code-scanning rule's TLSv1 heuristic)
    sctx = ssl.create_default_context(
        ssl.Purpose.CLIENT_AUTH)
    sctx.load_cert_chain(cert, key)
    srv.socket = sctx.wrap_socket(srv.socket, server_side=True)
    threading.Thread(target=srv.serve_forever, daemon=True).start()

    sock = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")
    nonce_file = os.path.join(work, "nonce.txt")
    keylog = os.path.join(work, "keys.log")
    cfg = os.path.join(work, "tls.json")
    with open(cfg, "w") as f:
        f.write(json.dumps({
            "redact": ["secret=*"],
            "intercept_scripts": [
                {"func_name": "SSL_write", "actions": [
                    {"action_name": "tls_content",
                     "action_params": {"dir": "w"}},
                    {"action_name": "call_real"}]},
                {"func_name": "SSL_write_ex", "actions": [
                    {"action_name": "tls_content",
                     "action_params": {"dir": "w"}},
                    {"action_name": "call_real"}]},
                {"func_name": "SSL_CTX_new", "actions": [
                    {"action_name": "call_real"},
                    {"action_name": "tls_keylog"}]},
            ],
        }))

    d = subprocess.Popen(
        [daemon, "--sock", sock, "--journal", journal,
         "--nonce-file", nonce_file],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        if not wait_sock(sock):
            print("FAIL: daemon never listened", file=sys.stderr)
            return 1
        with open(nonce_file) as f:
            nonce = f.read().strip()

        preload_var = ("DYLD_INSERT_LIBRARIES"
                       if sys.platform == "darwin" else "LD_PRELOAD")
        env = dict(os.environ)
        env.update({
            "RETRACE_SUPERVISOR": "1",
            "RETRACE_SUPERVISOR_SOCK": sock,
            "RETRACE_SUPERVISOR_NONCE": nonce,
            "RETRACE_SUPERVISOR_EAGER": "1",
            preload_var: rtlib,
            "RETRACE_JSON_CONFIG": cfg,
            "RETRACE_TLS_KEYLOG": keylog,
        })
        child = subprocess.run(
            [sys.executable, "-c", CLIENT,
             f"https://localhost:{port}/fetch?secret=hunter2"],
            env=env, capture_output=True, timeout=60)
        if child.returncode != 0:
            print(f"FAIL: client rc={child.returncode}: "
                  f"{child.stderr.decode()[-300:]}", file=sys.stderr)
            return 1

        d.send_signal(signal.SIGTERM)
        try:
            d.wait(timeout=5)
        except subprocess.TimeoutExpired:
            d.kill()

        recs = journal_records(journal)
        plains = [r.get("ev", {}) for r in recs
                  if r.get("ev", {}).get("name") == "retrace.tls.plain"]
        if not plains:
            print(f"FAIL: no retrace.tls.plain events: "
                  f"{[r.get('ev', {}).get('name') for r in recs]}",
                  file=sys.stderr)
            return 1
        line = plains[0].get("attrs", {}).get("line", "")
        if "GET /fetch" not in line:
            print(f"FAIL: request line missing: {line!r}",
                  file=sys.stderr)
            return 1
        if "hunter2" in line:
            print("FAIL: the secret leaked into evidence",
                  file=sys.stderr)
            return 1
        if "***" not in line:
            print(f"FAIL: the redaction star is missing: {line!r}",
                  file=sys.stderr)
            return 1

        # keylog arm: OpenSSL-only (LibreSSL has no callback slot)
        kl = ""
        if os.path.exists(keylog):
            with open(keylog, errors="replace") as f:
                kl = f.read()
        # TLS 1.2 logs CLIENT_RANDOM; TLS 1.3 logs the
        # HANDSHAKE_TRAFFIC_SECRET family -- any of them is a
        # captured secret
        if any(k in kl for k in ("CLIENT_RANDOM",
                                 "CLIENT_HANDSHAKE",
                                 "SERVER_HANDSHAKE")):
            print("keylog: secrets captured")
        elif "LibreSSL" in ssl.OPENSSL_VERSION:
            print("keylog: SKIP (LibreSSL has no callback slot)")
        else:
            print(f"FAIL: no keylog lines under "
                  f"{ssl.OPENSSL_VERSION}", file=sys.stderr)
            return 1

        print("tls_content: decrypted request line journaled, "
              "secret redacted -- OK")
        return 0
    finally:
        if d.poll() is None:
            d.send_signal(signal.SIGTERM)
            try:
                d.wait(timeout=5)
            except subprocess.TimeoutExpired:
                d.kill()
        srv.shutdown()


if __name__ == "__main__":
    sys.exit(main())
