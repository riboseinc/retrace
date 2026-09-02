#!/usr/bin/env python3
"""
E2E: noderetrace -- the Node runtime agent (the third adapter,
after pyretrace and jretrace; three hook systems, one
protocol). The child joins the daemon as a full peer (HELLO
with the nonce), observes its own runtime boundary through
diagnostics_channel, and direct-emits a marker; the journal
must carry the auth (full role, not spectator), the net
observation, and the marker.

Usage: test_noderetrace.py <retraced> <bindings/node>
"""
import json
import os
import subprocess
import sys
import tempfile
import time

from rpipe import journal_records, wait_sock

CHILD = r"""
const retrace = require(process.argv[1] + '/retrace.js');
async function main() {
  const joined = await retrace.supervise();
  if (!joined) {
    console.error('NO-OP (env absent) is wrong here');
    process.exit(3);
  }
  const fs = require('fs');
  const net = require('net');
  fs.readFileSync(process.argv[2]);   /* fs channel: node 20 */
  const srv = net.createServer();
  srv.listen(0, '127.0.0.1', () => {
    const c = net.connect(srv.address().port, '127.0.0.1');
    c.on('connect', () => {
      retrace.emit('node.test.marker', { kind: 'direct' });
      setTimeout(() => { c.destroy(); srv.close(); }, 300);
    });
  });
  setTimeout(() => process.exit(0), 1500);
}
main();
"""


def node_ok():
    """A usable Node is 18+ (diagnostics_channel stable floor).
    Any probe trouble -- a preview image with a half-installed
    toolchain, an unexpected output shape, a timeout -- is a
    SKIP, never a test failure: the gate's job is to decide
    whether the runtime exists, not to test the image."""
    try:
        r = subprocess.run(["node", "-e", "console.log(process.versions.node)"],
                           capture_output=True, text=True, timeout=15)
        if r.returncode != 0:
            return False
        out = r.stdout.strip()
        if not out or "." not in out:
            return False
        return int(out.split(".")[0]) >= 18
    except Exception:
        return False


def main():
    if len(sys.argv) != 3:
        print("usage: test_noderetrace.py <retraced> <bindings/node>",
              file=sys.stderr)
        return 2
    if not node_ok():
        print("SKIP: no Node 18+ on PATH", file=sys.stderr)
        return 0

    daemon, node_dir = (os.path.abspath(p) for p in sys.argv[1:3])
    work = tempfile.mkdtemp(prefix="nodert-")
    sock = (r"\\.\pipe\retrace-node-e2e" if os.name == "nt"
            else os.path.join(work, "agent.sock"))
    journal = os.path.join(work, "journal.jsonl")
    nonce_file = os.path.join(work, "nonce.txt")

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

        env = dict(os.environ)
        env.update({
            "RETRACE_SUPERVISOR": "1",
            "RETRACE_SUPERVISOR_SOCK": sock,
            "RETRACE_SUPERVISOR_NONCE": nonce,
        })
        sentinel = os.path.join(work, "sentinel.txt")
        with open(sentinel, "w") as f:
            f.write("node\n")
        child = subprocess.run(
            ["node", "-e", CHILD, node_dir, sentinel],
            env=env, capture_output=True, text=True, timeout=30)
        if child.returncode != 0:
            print(f"FAIL: child rc={child.returncode}: "
                  f"{child.stderr[:400]}", file=sys.stderr)
            return 1

        time.sleep(0.5)
        recs = journal_records(journal)
        names = [r.get("ev", {}).get("name") for r in recs]
        for want, label in (
                ("retrace.auth.agent", "the runtime agent's auth"),
                ("node.net.connect", "the socket observation"),
                ("node.test.marker", "the direct emit")):
            if want not in names:
                print(f"FAIL: {label} not journaled: {names}",
                      file=sys.stderr)
                return 1
        auth = [r for r in recs
                if r.get("ev", {}).get("name") == "retrace.auth.agent"]
        if auth and auth[-1]["ev"].get("role") == "spectator":
            print(f"FAIL: nonce peer seated as spectator: {auth}",
                  file=sys.stderr)
            return 1

        print("noderetrace: socket + direct emit journaled; "
              "full role -- OK", file=sys.stderr)
        return 0
    finally:
        if d.poll() is None:
            d.terminate()
            try:
                d.wait(timeout=5)
            except subprocess.TimeoutExpired:
                d.kill()


if __name__ == "__main__":
    sys.exit(main())
