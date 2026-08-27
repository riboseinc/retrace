#!/usr/bin/env python3
"""
E2E: jretrace, the JVM runtime agent (TODO.beyond-libc/04 P1).

This is the second runtime adapter after pyretrace, proving the
runtime-agent seam: a non-C, non-Python implementation speaks the
same RTRD supervisor protocol and lands source=runtime events in the
same hash-chained journal.

Usage: test_jretrace.py <retraced> <java-source-root>
"""
import json
import os
import re
import signal
import subprocess
import sys
import tempfile
import textwrap
import time


CHILD = r"""
import java.util.Map;
import org.retrace.runtime.JRetrace;

public class JRetraceSmoke {
    public static void main(String[] args) throws Exception {
        try (JRetrace rt = JRetrace.supervise()) {
            if (rt == null) {
                System.err.println("NO-OP (env absent) is wrong here");
                System.exit(3);
            }
            rt.fileRead(args[0]);
            rt.socketCreate("inet");
            rt.emit("jvm.test.marker", Map.of("kind", "direct"));
            Thread.sleep(1200L);
        }
    }
}
"""


def wait_sock(path, deadline=5.0):
    end = time.time() + deadline
    while time.time() < end:
        if os.path.exists(path):
            return True
        time.sleep(0.1)
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


def jdk_ok():
    """A usable JDK is one with the UDS API (Java 16+): on
    runners without java the exec itself raises FileNotFoundError
    (which used to crash the test instead of skipping), and older
    default JDKs (11) cannot compile UnixDomainSocketAddress."""
    for tool, args in (("java", ["-version"]),
                       ("javac", ["-version"])):
        try:
            r = subprocess.run([tool] + args, capture_output=True,
                               text=True, timeout=15)
        except (FileNotFoundError, subprocess.TimeoutExpired):
            return False
        if r.returncode != 0:
            return False
        # java: 'openjdk version "17.0.2"'; javac: 'javac 21.0.2';
        # legacy: 'java version "1.8.0_382"' (1 < 16 -> skip)
        blob = (r.stderr or "") + (r.stdout or "")
        m = (re.search(r'version\s+"(\d+)', blob) or
             re.search(r'javac\s+(\d+)', blob))
        if not m or int(m.group(1)) < 16:
            return False
    return True


def main():
    if len(sys.argv) != 3:
        print("usage: test_jretrace.py <retraced> <java-source-root>",
              file=sys.stderr)
        return 2
    if not jdk_ok():
        print("SKIP: no JDK 16+ on PATH", file=sys.stderr)
        return 0

    daemon, srcroot = (os.path.abspath(p) for p in sys.argv[1:3])
    work = tempfile.mkdtemp(prefix="jrt-")
    sock = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")
    nonce_file = os.path.join(work, "nonce.txt")
    sentinel = os.path.join(work, "sentinel.txt")
    classes = os.path.join(work, "classes")
    child_java = os.path.join(work, "JRetraceSmoke.java")
    os.mkdir(classes)
    with open(sentinel, "w") as f:
        f.write("jvm\n")
    with open(child_java, "w") as f:
        f.write(CHILD)

    javac = subprocess.run(
        ["javac", "-d", classes,
         os.path.join(srcroot, "org/retrace/runtime/JRetrace.java"),
         child_java],
        capture_output=True, text=True, timeout=30)
    if javac.returncode != 0:
        print(f"FAIL: javac rc={javac.returncode}: {javac.stderr[:400]}",
              file=sys.stderr)
        return 1

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
        child = subprocess.run(
            ["java", "-cp", classes, "JRetraceSmoke", sentinel],
            env=env, capture_output=True, text=True, timeout=30)
        if child.returncode != 0:
            print(f"FAIL: child rc={child.returncode}: "
                  f"{child.stderr[:400]}", file=sys.stderr)
            return 1

        d.send_signal(signal.SIGTERM)
        try:
            d.wait(timeout=5)
        except subprocess.TimeoutExpired:
            d.kill()

        recs = journal_records(journal)
        names = [r.get("ev", {}).get("name") for r in recs]
        for want, label in (
                ("jvm.file.read", "the JVM file read"),
                ("jvm.socket.create", "the JVM socket creation"),
                ("jvm.test.marker", "the direct JVM emit")):
            if want not in names:
                print(f"FAIL: {label} not journaled: {names}",
                      file=sys.stderr)
                return 1
        auth = [r for r in recs if r.get("ev", {}).get("name") ==
                "retrace.auth.agent"]
        if not auth or auth[-1]["ev"].get("role") != "full":
            print(f"FAIL: JVM runtime agent not full role: {auth}",
                  file=sys.stderr)
            return 1

        print("jretrace: file read + socket + direct emit journaled; "
              "JVM runtime agent a full peer -- OK")
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
