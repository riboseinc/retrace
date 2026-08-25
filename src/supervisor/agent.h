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
 * Post-boot hook, called at the very END of the constructor
 * chain (after as_init_late): the eager spawn happens HERE, not
 * in init -- a thread spawned mid-constructor dispatches
 * interposed calls (socket on Linux) through a half-initialized
 * engine and crashes the boot. Lazy spawn is unaffected (the
 * first emit always happens post-boot).
 */
void retrace_agent_post_boot(void);

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

/*
 * Apply a supervisor policy (TODO.supervisor/05): validate the
 * POLICY_SET payload (a policy header + a full retrace config),
 * swap it in as the ACTIVE config, and record its epoch.
 *
 * Payload shape (the daemon ships the policy file verbatim):
 *   {"policy": {"epoch": N, "expires": T|0},
 *    "intercept_scripts": [ ... retrace config ... ]}
 *
 * Fail-closed acceptance rules:
 *   - parses as JSON, carries a policy object and scripts;
 *   - epoch is strictly greater than the applied one (replay
 *     protection -- an old epoch is refused even if the daemon
 *     repeats it);
 *   - expires == 0 (never) or in the future.
 * A refused policy changes nothing; the caller reports it via
 * POLICY_ACK so the daemon's audit trail shows the refusal.
 *
 * Returns 0 applied, -1 refused (reason copied into reason_out
 * when non-NULL). Also callable directly (unit tests).
 */
int retrace_agent_policy_apply(const char *payload_json,
	char *reason_out, size_t reason_cap);

#endif /* RETRACE_SUPERVISOR_AGENT_H_ */
