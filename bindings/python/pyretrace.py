"""pyretrace -- the Python runtime agent (TODO.beyond-libc/04).

A THIRD-PARTY implementation of the retrace supervisor protocol
(the same framing and state machine the conformance suite's
reference stub walks). sys.audit hooks give the runtime's own
syscall-ish boundary: file opens, sockets, execs -- attributed
to the MODULE that issued them, the layer a libc interposer
sees only as "an open from pid N".

    import pyretrace
    pyretrace.supervise()          # joins RETRACE_SUPERVISOR env

Zero-mandatory-config: with the env absent, supervise() is a
no-op (the preload plane's own gating doctrine). The agent
registers as source=runtime; the labeler keeps libc / kernel /
runtime as lanes of one session.

Wire contract: RTRD framing, message ids parsed from protocol.h
at runtime when reachable (repo checkouts), else the frozen v1
table below -- the conformance suite pins both to the same
artifacts.
"""

import json
import os
import socket
import struct
import sys
import threading
import time

_MAGIC = b"RTRD"
# frozen v1 table (see src/supervisor/protocol.h; the
# conformance suite asserts these never drift)
_HELLO, _HEARTBEAT, _EVENT, _BYE = 1, 2, 4, 6

_STATE = {
    "sock": None,
    "agent_id": "pending",
    "seq": 0,
    "lock": threading.Lock(),
    "thread": None,
    "stop": False,
}


def _frame(mid, payload):
    b = payload.encode()
    return _MAGIC + struct.pack("<HHI", 1, mid, len(b)) + b


def _send(mid, obj):
    s = _STATE["sock"]
    if s is None:
        return
    try:
        s.sendall(_frame(mid, json.dumps(obj, sort_keys=True)))
    except OSError:
        with _STATE["lock"]:
            _STATE["sock"] = None


def _recv_exact(s, n, deadline):
    buf = b""
    while len(buf) < n:
        s.settimeout(max(0.1, deadline - time.time()))
        chunk = s.recv(n - len(buf))
        if not chunk:
            raise EOFError
        buf += chunk
    return buf


def _hello(sock_path, nonce):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock_path)
    s.sendall(_frame(_HELLO, json.dumps({
        "session_token": os.environ.get("RETRACE_SESSION", ""),
        "nonce": nonce, "pid": os.getpid(), "ppid": os.getppid(),
        "boot_id": "pyruntime", "cmdline": " ".join(sys.argv[:4]),
        "retrace_version": "pyretrace-1"})))
    hdr = _recv_exact(s, 12, time.time() + 5)
    _, _, mid, ln = struct.unpack("<4sHHI", hdr)
    body = _recv_exact(s, ln, time.time() + 5)
    if mid != 16:  # WELCOME
        s.close()
        raise RuntimeError(f"expected WELCOME, got {mid}")
    welcome = json.loads(body)
    # a POLICY_SET may follow; runtime agents are observers --
    # read and drop it
    s.settimeout(0.2)
    try:
        hdr = s.recv(12)
        if len(hdr) == 12:
            _, _, mid2, ln2 = struct.unpack("<4sHHI", hdr)
            if ln2:
                _recv_exact(s, ln2, time.time() + 1)
    except OSError:
        pass
    return s, welcome


def _agent_loop(sock_path, nonce):
    backoff = 0.5
    while not _STATE["stop"]:
        try:
            s, welcome = _hello(sock_path, nonce)
        except (OSError, EOFError, RuntimeError):
            time.sleep(backoff)
            backoff = min(backoff * 2, 10.0)
            continue
        backoff = 0.5
        with _STATE["lock"]:
            _STATE["sock"] = s
            _STATE["agent_id"] = welcome.get("agent_id",
                                             "pending")
        while not _STATE["stop"]:
            time.sleep(1.0)
            _send(_HEARTBEAT, {"agent_id": _STATE["agent_id"],
                               "seq": _STATE["seq"]})
            if _STATE["sock"] is None:
                break
        with _STATE["lock"]:
            if _STATE["sock"] is not None:
                _send(_BYE, {"agent_id": _STATE["agent_id"]})
                _STATE["sock"].close()
                _STATE["sock"] = None


def emit(name, **attrs):
    """one runtime-attributed event (public API)"""
    with _STATE["lock"]:
        _STATE["seq"] += 1
        seq = _STATE["seq"]
        agent = _STATE["agent_id"]
    payload = {"agent_id": agent, "seq": seq,
               "ts": int(time.time()), "name": name,
               "attrs": {k: str(v) for k, v in attrs.items()},
               "source": "runtime"}
    _send(_EVENT, payload)


def _audit_hook(event, args):
    if event == "open":
        path, mode, flags = args[0], args[1], args[2]
        if any(c in (mode or "") for c in "wa+") or \
           (flags is not None and flags & 0x41 and
            (mode or "r") != "r"):
            emit("py.file.write", path=str(path))
        else:
            emit("py.file.read", path=str(path))
    elif event in ("socket.__new__",):
        emit("py.socket.create", family=args[0])
    elif event == "subprocess.Popen":
        emit("py.exec", argv=[str(a) for a in args[0][:4]])
    elif event == "os.system":
        emit("py.exec", command=str(args[0])[:120])


def supervise():
    """join the supervisor named by RETRACE_SUPERVISOR env; a
    no-op when absent"""
    if os.environ.get("RETRACE_SUPERVISOR") != "1":
        return False
    sock_path = os.environ.get("RETRACE_SUPERVISOR_SOCK")
    nonce = os.environ.get("RETRACE_SUPERVISOR_NONCE", "")
    if not sock_path:
        return False
    if _STATE["thread"] is not None:
        return True
    _STATE["thread"] = threading.Thread(
        target=_agent_loop, args=(sock_path, nonce), daemon=True)
    _STATE["thread"].start()
    # settle past HELLO/WELCOME so early emits carry the id
    deadline = time.time() + 3
    while time.time() < deadline and _STATE["sock"] is None:
        time.sleep(0.05)
    try:
        sys.addaudithook(_audit_hook)
    except Exception:
        pass
    return True


def stop():
    """detach (tests)"""
    _STATE["stop"] = True
    t = _STATE["thread"]
    if t is not None:
        t.join(timeout=3)
