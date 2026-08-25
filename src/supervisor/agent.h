/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */
#ifndef RETRACE_SUPERVISOR_AGENT_H_
#define RETRACE_SUPERVISOR_AGENT_H_

#include <stddef.h>

/*
 * The in-process control agent (TODO.supervisor/03) -- the
 * THIRD permanent-guard background thread (after the log
 * flusher and the otlp tick thread). It connects the traced
 * process to the retraced daemon: HELLO at attach,
 * HEARTBEATs with the event sequence, named security events
 * on the plan-01 protocol, PING responder.
 *
 * The laws (Wave B/C, restated):
 *   - OFF THE HOT PATH: emit_event only enqueues (bounded,
 *     drop-with-count); the agent thread owns the socket.
 *   - PERMANENT GUARD + logging-disabled BEFORE any
 *     interposable call on the agent thread.
 *   - ONE SPAWNER: CAS; lazy on the first event, never in the
 *     constructor (the musl hazard).
 *   - FAIL-OPEN LIVENESS: daemon unreachable -> backoff and
 *     retry (0.5s..30s, jittered); NEVER exit the process,
 *     NEVER unhook. deinit flushes pending events with a
 *     bounded budget so short-lived targets still deliver.
 *
 * Gating: RETRACE_SUPERVISOR=1 arms the agent (checked once at
 * init); RETRACE_SUPERVISOR_SOCK overrides the default socket
 * path. Unset -> init is a silent no-op and the library is
 * byte-for-byte its old self.
 */

int retrace_agent_init(void);
void retrace_agent_deinit(void);

/*
 * Queue one named security event for the daemon. kv is an
 * array of n_kv PAIRS of strings (key, value) -- the wire
 * shape is the protocol's EVENT message (attrs: object).
 * Bounded: on a full queue the event is dropped and counted,
 * never blocking the caller. Returns 0 (enqueued or armed-off
 * or dropped-counted), -1 on invalid args.
 */
int retrace_agent_emit_event(const char *name,
	const char *const *kv, size_t n_kv);

#endif /* RETRACE_SUPERVISOR_AGENT_H_ */
