#!/usr/bin/env python3
"""
Integration test for otlp-c live streaming (TODO.trace-profile/31).

Spawns the fixture HTTP server, runs a known traced target under
retrace v2 with RETRACE_OTLP_ENDPOINT pointing at the fixture,
and asserts that at least one span was POSTed to /v1/traces.

Usage:
    test_otlp_live.py <retrace_dylib> <target_binary> <fixture_server>

Exits 0 on success, non-zero on failure.
"""
import os
import selectors
import socket
import subprocess
import sys
import tempfile
import time

# Find a free port to avoid clashes with other tests.
def find_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def main():
    if len(sys.argv) != 4:
        print("usage: test_otlp_live.py <retrace_dylib> <target> <fixture>",
              file=sys.stderr)
        return 2

    dylib = os.path.abspath(sys.argv[1])
    target = os.path.abspath(sys.argv[2])
    fixture = os.path.abspath(sys.argv[3])

    if not os.path.exists(dylib):
        print(f"FAIL: retrace library not found: {dylib}", file=sys.stderr)
        return 2
    if not os.path.exists(target):
        print(f"FAIL: target binary not found: {target}", file=sys.stderr)
        return 2
    if not os.path.exists(fixture):
        print(f"FAIL: fixture server not found: {fixture}", file=sys.stderr)
        return 2

    port = find_free_port()
    request_log = tempfile.NamedTemporaryFile(
        prefix="otlp-req-", suffix=".jsonl", delete=False)
    request_log.close()

    # Start the fixture server. Prefer the SYSTEM interpreter for
    # the child: Homebrew's framework-python builds (3.14 on the
    # macos-15/26 CI images) hang before main() when spawned with
    # redirected pipes -- no stdout, no listen socket, process
    # alive (observed: 20s deadline, rc=-15, empty pipes). The
    # fixture is plain stdlib; /usr/bin/python3 (3.9+) runs it
    # fine everywhere.
    child_python = "/usr/bin/python3"
    if not os.path.exists(child_python):
        child_python = sys.executable
    server = subprocess.Popen(
        [child_python, fixture, str(port), request_log.name],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        # Readiness handshake: the server prints one "listening"
        # line (flush=True) after bind(). Read via selectors with
        # a deadline -- a bare readline() would block forever if
        # the server hangs before printing.
        sel = selectors.DefaultSelector()
        sel.register(server.stdout, selectors.EVENT_READ)
        ready_line = None
        buf = b""
        deadline = time.time() + 20.0
        while time.time() < deadline:
            if server.poll() is not None:
                out, err = server.communicate(timeout=5)
                print(f"FAIL: fixture server exited early "
                      f"(rc={server.returncode})\n"
                      f"  stdout: {out!r}\n  stderr: {err!r}",
                      file=sys.stderr)
                return 1
            if sel.select(0.2):
                chunk = os.read(server.stdout.fileno(), 4096)
                if not chunk:
                    break
                buf += chunk
                if b"\n" in buf:
                    ready_line = buf.split(b"\n", 1)[0].decode(
                        errors="replace")
                    break
        sel.close()
        if ready_line is None or not ready_line.startswith(
                "fixture_otlp_server:"):
            server.terminate()
            try:
                out, err = server.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                out, err = server.communicate()
            print(f"FAIL: fixture server did not report ready "
                  f"(rc={server.poll()})\n"
                  f"  stdout: {out!r}\n  stderr: {err!r}",
                  file=sys.stderr)
            return 1
        # Secondary probe: connect once to confirm listen().
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=2.0):
                pass
        except OSError as e:
            server.terminate()
            out, err = server.communicate(timeout=5)
            print(f"FAIL: fixture server accepted no connection "
                  f"({e!r})\n  stdout: {out!r}\n  stderr: {err!r}",
                  file=sys.stderr)
            return 1

        # Run the target under v2 with the live endpoint set.
        env = os.environ.copy()
        env["RETRACE_OTLP_ENDPOINT"] = f"http://127.0.0.1:{port}"
        env["RETRACE_LOGGER_DEF_ENA"] = "1"
        env["RETRACE_LOGGER_DEF_STDOUT_ENA"] = "0"
        env["RETRACE_LOGGER_FMT"] = "jsonl"
        # Use the target with stdout redirected to a file so we can
        # check both the target's output and the otlp stats line.
        target_log = tempfile.NamedTemporaryFile(
            prefix="otlp-target-", suffix=".log", delete=False)
        target_log.close()
        # Need a log destination to spin up the flusher (which
        # feeds otlp_live_emit_json). The file is not used for
        # assertions -- the fixture server is -- but without it,
        # the logger's ring subsystem stays uninitialized and
        # spans are never pushed to otlp-c.
        env["RETRACE_LOGGER_DEF_FN"] = target_log.name + ".log"
        # The dylib is injected via LD_PRELOAD/DYLD_INSERT_LIBRARIES
        # by the platform's trampoline -- we set the env var here
        # so ctest doesn't need to know which one.
        if "DYLD_INSERT_LIBRARIES" not in env and "LD_PRELOAD" not in env:
            if sys.platform == "darwin":
                env["DYLD_INSERT_LIBRARIES"] = dylib
            else:
                env["LD_PRELOAD"] = dylib

        # Use the target with stdout redirected to a file so we can
        # check both the target's output and the otlp stats line.
        try:
            with open(target_log.name, "wb") as tf:
                proc = subprocess.run(
                    [target], env=env, stdout=tf, stderr=subprocess.PIPE,
                    timeout=30)
        except subprocess.TimeoutExpired:
            print("FAIL: target timed out", file=sys.stderr)
            return 1

        # The destructor's bounded flush sends everything before
        # the target exits, so the server has already recorded
        # the POSTs; 1s of slack for slow runners.
        time.sleep(1.0)

        # Read the request log.
        with open(request_log.name) as f:
            lines = f.read().splitlines()

        # Read the target's stderr for the otlp stats line.
        with open(target_log.name, "rb") as f:
            target_out = f.read().decode("utf-8", errors="replace")
        stderr = (proc.stderr or b"").decode("utf-8", errors="replace")

        if not lines:
            print(f"FAIL: no requests received at fixture server "
                  f"(target exit={proc.returncode} stderr: {stderr[:200]!r})",
                  file=sys.stderr)
            print(f"  request_log: {request_log.name} (exists: "
                  f"{os.path.exists(request_log.name)})",
                  file=sys.stderr)
            print(f"  target_log: {target_log.name} (exists: "
                  f"{os.path.exists(target_log.name)})",
                  file=sys.stderr)
            return 1

        # Each request is a JSON line. We expect at least one
        # POST to /v1/traces with protobuf content.
        proto_posts = sum(
            1 for ln in lines if '"content_type": "application/x-protobuf"' in ln)
        if proto_posts < 1:
            print(f"FAIL: no protobuf POSTs (got {len(lines)} total "
                  f"requests, 0 protobuf)", file=sys.stderr)
            return 1

        # Check the stats line in stderr.
        if "otlp_live: emitted=" not in stderr:
            print(f"FAIL: no otlp_live stats line in stderr "
                  f"(target exit={proc.returncode})\n"
                  f"  stderr: {stderr[:400]!r}\n"
                  f"  target stdout tail: {target_out[-200:]!r}",
                  file=sys.stderr)
            return 1
        # And that sent > 0.
        for line in stderr.splitlines():
            if "otlp_live:" in line and "sent=0" in line:
                # Acceptable only if the target finished before
                # the first tick could complete -- but with our
                # 100ms tick + 30s timeout, that should not happen.
                print(f"WARN: spans were queued but 0 sent: {line}",
                      file=sys.stderr)

        print(f"PASS: {proto_posts} protobuf POST(s) to fixture server; "
              f"{len(lines)} total request(s)")
        return 0
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
        try:
            os.unlink(request_log.name)
        except OSError:
            pass


if __name__ == "__main__":
    sys.exit(main())
