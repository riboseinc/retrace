/*
 * retrace -- the Node runtime agent.
 *
 * A THIRD-PARTY implementation of the retrace supervisor
 * protocol (the same RTRD framing and state machine the
 * conformance suite's reference stub walks -- the third
 * runtime adapter after pyretrace and jretrace, three hook
 * systems, one protocol). diagnostics_channel gives the
 * runtime's own syscall-ish boundary -- file access, sockets,
 * child processes -- attributed as the runtime lane.
 *
 *   const retrace = require('retrace')
 *   retrace.supervise()        // joins RETRACE_SUPERVISOR env
 *
 * Zero-mandatory-config: with the env absent, supervise() is a
 * no-op (the preload plane's own gating doctrine). Events are
 * source=runtime; the labeler keeps libc / kernel / runtime as
 * lanes of one session.
 */

'use strict';

const fs = require('fs');
const net = require('net');
const os = require('os');

const MAGIC = Buffer.from('RTRD');
/* frozen v1 table (see src/supervisor/protocol.h; the
 * conformance suite asserts these never drift) */
const HELLO = 1, HEARTBEAT = 2, EVENT = 4, BYE = 6, WELCOME = 16;

const PIPE_PREFIX = '\\\\.\\pipe\\';

const state = {
  sock: null,        /* fd (POSIX) | handle (Windows) */
  agentId: 'pending',
  seq: 0,
  stop: false,
  heartbeat: null,
  isPipe: false,
};

function frame(mid, payload) {
  const body = Buffer.from(JSON.stringify(payload));
  const hdr = Buffer.alloc(12);
  MAGIC.copy(hdr, 0);
  hdr.writeUInt16LE(1, 4);
  hdr.writeUInt16LE(mid, 6);
  hdr.writeUInt32LE(body.length, 8);
  return Buffer.concat([hdr, body]);
}

function send(mid, payload) {
  if (!state.sock)
    return;
  const buf = frame(mid, payload);
  try {
    if (state.isPipe)
      fs.writeSync(state.sock, buf);
    else
      state.sock.write(buf);
  } catch (_) {
    state.sock = null;
  }
}

function emit(name, attrs) {
  state.seq += 1;
  send(EVENT, {
    agent_id: state.agentId,
    seq: state.seq,
    ts: Math.floor(Date.now() / 1000),
    name: name,
    attrs: Object.fromEntries(
      Object.entries(attrs || {}).map(([k, v]) => [k, String(v)])),
    source: 'runtime',
  });
}

/* ---- the runtime's own boundary (best-effort: the
 * diagnostics_channel surface churned across Node versions,
 * so each subscription is guarded and its absence is not an
 * error -- the direct emit() API is the stable floor) ---- */
function observeRuntime() {
  let dc = null;
  try {
    dc = require('diagnostics_channel');
  } catch (_) {
    return;
  }
  const on = (ch, name, pick) => {
    try {
      dc.subscribe(ch, (m) => {
        const attrs = pick(m);
        if (attrs)
          emit(name, attrs);
      });
    } catch (_) { /* channel absent on this runtime */ }
  };
  /* the fs channel surface: Node 18's 'fs.sync'/'fs.async'
   * carry {path}; Node 22+ split per-operation with the path
   * on the request -- every spelling is subscribed, guarded,
   * and read for either shape */
  const fsPick = (m) => {
    const p = m && (m.path ||
      (m.request && m.request.path) ||
      (m.response && m.response.path));
    return p ? { path: p } : null;
  };
  const fsRead = (m) => {
    const a = fsPick(m);
    return a;
  };
  for (const ch of ['fs.sync', 'fs.async', 'fs.sync.read',
    'fs.async.read'])
    on(ch, 'node.file.read', fsRead);
  for (const ch of ['fs.sync.write', 'fs.async.write'])
    on(ch, 'node.file.write', fsPick);
  on('net.client.socket', 'node.net.connect', () => ({}));
  on('child_process.run', 'node.exec.spawn', () => ({}));
}

function readWelcome(sockPath, nonce) {
  return new Promise((resolve, reject) => {
    const finish = (sock, isPipe) => {
      const chunks = [];
      const take = () => {
        const buf = Buffer.concat(chunks);
        if (buf.length < 12)
          return false;
        const len = buf.readUInt32LE(8);
        if (buf.length < 12 + len)
          return false;
        const mid = buf.readUInt16LE(6);
        const payload =
          JSON.parse(buf.subarray(12, 12 + len).toString());
        if (mid !== WELCOME) {
          reject(new Error('expected WELCOME, got ' + mid));
          return true;
        }
        resolve({ sock, isPipe, payload });
        return true;
      };
      if (isPipe) {
        let acc = Buffer.alloc(0);
        const step = () => {
          let got;
          let b;
          try {
            b = Buffer.alloc(4096);
            got = fs.readSync(sock, b, 0, b.length, null);
            if (got <= 0)
              throw new Error('pipe closed');
          } catch (e) {
            reject(e);
            return;
          }
          acc = Buffer.concat([acc, b.subarray(0, got)]);
          chunks.push(b.subarray(0, got));
          if (!take())
            setImmediate(step);
        };
        /* HELLO first */
        try {
          fs.writeSync(sock, frame(HELLO, {
            session_token: process.env.RETRACE_SESSION || '',
            nonce: nonce,
            pid: process.pid,
            ppid: 0,
            boot_id: 'noderuntime',
            cmdline: process.argv.slice(0, 4).join(' '),
            retrace_version: 'noderetrace-1',
          }));
        } catch (e) {
          reject(e);
          return;
        }
        step();
      } else {
        sock.on('data', (b) => {
          chunks.push(b);
          take();
        });
        sock.on('error', reject);
        sock.write(frame(HELLO, {
          session_token: process.env.RETRACE_SESSION || '',
          nonce: nonce,
          pid: process.pid,
          ppid: 0,
          boot_id: 'noderuntime',
          cmdline: process.argv.slice(0, 4).join(' '),
          retrace_version: 'noderetrace-1',
        }));
      }
    };
    if (sockPath.startsWith(PIPE_PREFIX)) {
      fs.open(sockPath, 'r+', (err, fd) => {
        if (err) {
          reject(err);
          return;
        }
        finish(fd, true);
      });
    } else {
      const s = net.connect({ path: sockPath });
      s.on('connect', () => finish(s, false));
      s.on('error', reject);
    }
  });
}

async function supervise() {
  if (process.env.RETRACE_SUPERVISOR !== '1')
    return false;
  const sockPath = process.env.RETRACE_SUPERVISOR_SOCK || '';
  const nonce = process.env.RETRACE_SUPERVISOR_NONCE || '';
  if (!sockPath)
    return false;

  let backoff = 500;
  while (!state.stop) {
    try {
      const r = await readWelcome(sockPath, nonce);
      state.sock = r.sock;
      state.isPipe = r.isPipe;
      state.agentId = r.payload.agent_id || 'pending';
      break;
    } catch (_) {
      await new Promise((res) => setTimeout(res, backoff));
      backoff = Math.min(backoff * 2, 10000);
    }
  }

  state.heartbeat = setInterval(() => {
    send(HEARTBEAT, { agent_id: state.agentId, seq: state.seq });
  }, 1000);

  observeRuntime();

  const bye = () => {
    if (state.stop)
      return;
    state.stop = true;
    clearInterval(state.heartbeat);
    send(BYE, { agent_id: state.agentId });
    try {
      if (state.isPipe)
        fs.closeSync(state.sock);
      else
        state.sock.end();
    } catch (_) { /* teardown best-effort */ }
  };
  process.on('exit', bye);
  process.on('SIGTERM', () => { bye(); process.exit(0); });
  process.on('SIGINT', () => { bye(); process.exit(0); });
  return true;
}

module.exports = { supervise, emit };
