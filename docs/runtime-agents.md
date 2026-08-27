# Writing a retrace runtime agent

A *runtime agent* attributes runtime-internal semantics onto a retrace
supervisor session: which module opened the file, which line issued the
connect — the layer a libc interposer sees only as "an open from pid
N". Two reference implementations ship in this tree:

- **pyretrace** (`bindings/python/pyretrace.py`) — Python, `sys.audit`
  hooks. ~190 lines, no dependencies.
- **jretrace** (`bindings/jvm/src/main/java/org/retrace/runtime/JRetrace.java`)
  — Java 16+, `java.net.UnixDomainSocketAddress`. No JNI, no external
  libraries.

Both are accepted by the protocol conformance suite
(`test/conformance/`) unchanged — that suite is the acceptance test for
any third-party agent.

## The contract

### Activation: the environment

The spawn environment carries everything the agent needs; with the env
absent, `supervise()` is a no-op (zero-mandatory-config):

| Variable | Meaning |
|----------|---------|
| `RETRACE_SUPERVISOR=1` | opt-in gate; absent ⇒ no-op |
| `RETRACE_SUPERVISOR_SOCK` | the daemon's agent socket path |
| `RETRACE_SUPERVISOR_NONCE` | the channel nonce (full role) |
| `RETRACE_SESSION` | session token (optional) |

The nonce arrives out-of-band from the spawner (`retraced
--nonce-file`). Presenting it in HELLO seats the agent as a **full**
peer; omitting it seats a **spectator** — evidence flows, policy and
commands never do. That is not a failure mode: kernel-observation
agents (eBPF, ETW) deliberately HELLO without the nonce.

### Transport: RTRD framing over UDS

Little-endian 12-byte header + JSON payload:

```
"RTRD" | uint16 version (=1) | uint16 type | uint32 payload_len
```

Message ids (single source of truth: `src/supervisor/protocol.h`;
the conformance suite greps that block — these values are frozen):

| id | Message | Direction |
|----|---------|-----------|
| 1 | HELLO | agent → daemon |
| 2 | HEARTBEAT | agent → daemon |
| 3 | POLICY_ACK | agent → daemon |
| 4 | EVENT | agent → daemon |
| 5 | RING_DATA | agent → daemon |
| 6 | BYE | agent → daemon |
| 16 | WELCOME | daemon → agent |
| 17 | POLICY_SET | daemon → agent |
| 18 | CMD | daemon → agent |
| 19 | PING | daemon → agent |

### The conversation

1. Connect, send **HELLO** (pid, ppid, cmdline, nonce, boot id).
2. Read **WELCOME** — it carries your `agent_id`; quote it in every
   later message.
3. A **POLICY_SET** may follow immediately. An observer reads and
   drops it; a preload-policy consumer applies it and answers
   **POLICY_ACK** with the epoch.
4. Send **HEARTBEAT** (agent_id, last seq) about once a second; the
   daemon sweeps stale agents after ~15 s.
5. Send **EVENT** on activity: `{"agent_id", "seq" (monotonic),
   "ts", "name", "attrs", "source"}`. Set `"source"` to your lane —
   `"libc"`, `"kernel"`, or `"runtime"` — so the journal keeps the
   lanes distinct.
6. On shutdown send **BYE**.

Event names are dotted: the Python agent emits `py.file.read`,
`py.socket.create`; the JVM agent `jvm.file.read`,
`jvm.socket.create`; the kernel agents `kernel.syscall.observe`.

## Doctrine (the two rules that are not negotiable)

1. **Never block or crash the host process.** The agent thread is a
   daemon; a dead supervisor means "no control channel," nothing more.
2. **Observers ride as spectators.** If your agent cannot honor the
   POLICY_SET discipline (journal-chain acks, epochs), HELLO without
   the nonce deliberately. Evidence always; policy never.

## Testing your agent

- Point `RETRACE_SUPERVISOR_SOCK` at a `retraced` started with
  `--nonce-file`, run your workload, stop the daemon (SIGTERM), and
  read the journal JSONL: your events must appear with your `source`
  lane, and `retrace.auth.agent` must show the role you intended.
- The integration tests `test_pyretrace.py` / `test_jretrace.py` show
  the full pattern in ~120 lines each.

## Beyond UDS: other lanes

Kernel-observation agents (`retrace-ebpf-agent`,
`retrace-etw-agent`) speak the same framing as spectators with
`source: kernel`. The Windows named-pipe transport
(TODO.supervisor/12) carries the same protocol; until it lands, the
ETW lane rides the `etw2retrace` loader path.
