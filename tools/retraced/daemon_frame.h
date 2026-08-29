/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_TOOLS_RETRACED_DAEMON_FRAME_H_
#define RETRACE_TOOLS_RETRACED_DAEMON_FRAME_H_

#include <stdint.h>

#include "journal.h"
#include "registry.h"
#include "retraced_ctl.h"

/*
 * The daemon frame-dispatch module (the architecture review's A):
 * ONE protocol state machine behind both daemon transports. The
 * POSIX poll loop and the Windows pipe threads each own accept/
 * read machinery ONLY; the rules -- nonce roles, WELCOME minting,
 * event journaling, kernel-lane drift counting, policy ACKs,
 * PING -- live here. Transports embed daemon_conn_state in their
 * connection struct and pass their write_frame seam.
 */

struct daemon_conn_state {
	char agent_id[RETRACED_AGENT_ID_MAX];
	int helloed;
	int spectator;
};

/*
 * Handle one decoded frame. Returns 0 to continue, 1 when the
 * agent said BYE (transport drops the connection), -1 on a
 * protocol violation (drop). kernel-lane drift counting lives on
 * the REGISTRY ENTRY -- one site for both transports.
 */
int daemon_frame_handle(struct daemon_conn_state *st, uint16_t type,
	const char *payload, long now_ms,
	struct retraced_registry *reg, struct retraced_journal *jr,
	const struct retraced_ctl_ctx *ctl, const char *daemon_nonce,
	int (*write_frame)(void *io, uint16_t type,
		const char *payload),
	void *io);

/* HELLO completed: mint + write WELCOME, journal the auth line */
void daemon_frame_welcome(const struct daemon_conn_state *st,
	const struct retraced_ctl_ctx *ctl,
	int (*write_frame)(void *io, uint16_t type,
		const char *payload),
	void *io, struct retraced_journal *jr);

/* the heartbeat-grade drift summaries (sweeps + shutdown) */
void daemon_frame_drift_summaries(struct retraced_registry *reg,
	struct retraced_journal *jr);

#endif /* RETRACE_TOOLS_RETRACED_DAEMON_FRAME_H_ */
