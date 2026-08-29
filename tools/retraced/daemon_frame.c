/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * The shared daemon state machine (see daemon_frame.h). Kept
 * transport-blind: no sockets, no pipes, no locks -- the callers
 * serialize access (single poll loop / global critical section).
 * The conformance suite and the three daemon E2Es pin this module
 * from both transports.
 */

#include "daemon_frame.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "protocol.h"

#include "parson.h"

static void jstr(JSON_Object *o, const char *key, char *dst,
	size_t cap)
{
	const char *v = json_object_get_string(o, key);

	dst[0] = '\0';
	if (v != NULL)
		snprintf(dst, cap, "%s", v);
}

void daemon_frame_welcome(const struct daemon_conn_state *st,
	const struct retraced_ctl_ctx *ctl,
	int (*write_frame)(void *io, uint16_t type,
		const char *payload),
	void *io, struct retraced_journal *jr)
{
	char w[256];
	char ev[320];

	snprintf(w, sizeof(w),
		"{\"agent_id\":\"%s\",\"epoch\":%ld,\"role\":\"%s\"}",
		st->agent_id, (long)ctl->policy_epoch,
		st->spectator ? "spectator" : "full");
	write_frame(io, RETRACE_RPC_MSG_WELCOME, w);

	snprintf(ev, sizeof(ev),
		"{\"name\":\"retrace.auth.agent\",\"agent\":\"%s\",\"role\":\"%s\",\"nonce\":\"%s\"}",
		st->agent_id, st->spectator ? "spectator" : "full",
		st->spectator ? "" : "redacted");
	retraced_journal_event(jr, (long)time(NULL), "daemon", 0, ev);
}

void daemon_frame_drift_summaries(struct retraced_registry *reg,
	struct retraced_journal *jr)
{
	size_t k;

	for (k = 0; k < reg->count; k++) {
		struct agent_entry *e = &reg->agents[k];
		char ev[192];

		if (e->kernel_obs == 0 ||
		    e->kernel_obs == e->kernel_obs_last)
			continue;
		snprintf(ev, sizeof(ev),
			"{\"name\":\"retrace.drift.summary\",\"agent\":\"%s\",\"kernel_obs\":%llu,\"delta\":%llu}",
			e->id,
			(unsigned long long)e->kernel_obs,
			(unsigned long long)(e->kernel_obs -
				e->kernel_obs_last));
		retraced_journal_event(jr, (long)time(NULL),
			"daemon", 0, ev);
		e->kernel_obs_last = e->kernel_obs;
	}
}

int daemon_frame_handle(struct daemon_conn_state *st, uint16_t type,
	const char *payload, long now_ms,
	struct retraced_registry *reg, struct retraced_journal *jr,
	const struct retraced_ctl_ctx *ctl, const char *daemon_nonce,
	int (*write_frame)(void *io, uint16_t type,
		const char *payload),
	void *io)
{
	JSON_Value *v;
	JSON_Object *o;

	switch (type) {
	case RETRACE_RPC_MSG_HEARTBEAT: {
		if (!st->helloed)
			return 0;
		v = json_parse_string(payload);
		if (v == NULL)
			return 0;
		o = json_value_get_object(v);
		retraced_registry_heartbeat(reg, st->agent_id,
			(uint64_t)json_object_get_number(o, "seq"),
			now_ms);
		json_value_free(v);
		return 0;
	}
	case RETRACE_RPC_MSG_EVENT: {
		const char *source;

		if (!st->helloed)
			return 0;
		v = json_parse_string(payload);
		if (v == NULL)
			return 0;
		o = json_value_get_object(v);
		retraced_journal_event(jr, (long)time(NULL),
			st->agent_id,
			(uint64_t)json_object_get_number(o, "seq"),
			payload);
		/*
		 * Live drift grading (TODO.beyond-libc/03 P1):
		 * kernel-lane observations count on the REGISTRY
		 * ENTRY -- one site, both transports (the Windows
		 * copy once counted on the pipe conn while the
		 * summaries read the entry: never met).
		 */
		source = json_object_get_string(o, "source");
		if (source != NULL && strcmp(source, "kernel") == 0) {
			struct agent_entry *e =
				retraced_registry_find(reg,
					st->agent_id);

			if (e != NULL)
				e->kernel_obs++;
		}
		json_value_free(v);
		return 0;
	}
	case RETRACE_RPC_MSG_POLICY_ACK: {
		struct agent_entry *e;

		if (!st->helloed)
			return 0;
		v = json_parse_string(payload);
		if (v != NULL) {
			o = json_value_get_object(v);
			e = retraced_registry_find(reg, st->agent_id);
			if (e != NULL)
				e->policy_epoch = (uint64_t)
					json_object_get_number(o,
						"policy_epoch");
			json_value_free(v);
		}
		retraced_journal_event(jr, (long)time(NULL),
			st->agent_id, 0, payload);
		return 0;
	}
	case RETRACE_RPC_MSG_PING:
		write_frame(io, RETRACE_RPC_MSG_PING, "{}");
		return 0;
	case RETRACE_RPC_MSG_BYE:
		retraced_registry_bye(reg, st->agent_id);
		st->helloed = 0;
		return 1;
	default:
		return 0;
	}
}

/*
 * HELLO is the one transport-divergent arm: the POSIX daemon
 * stitches session trees and pushes policy on registration; the
 * Windows P0 mints the entry directly. Both end at
 * daemon_frame_welcome. This helper does the shared half: nonce
 * role + registry mint + welcome/auth emit, returning the entry
 * (NULL = registry full).
 */
struct agent_entry *daemon_frame_hello(struct daemon_conn_state *st,
	const char *payload, struct retraced_registry *reg,
	struct retraced_journal *jr, const struct retraced_ctl_ctx *ctl,
	const char *daemon_nonce,
	int (*write_frame)(void *io, uint16_t type,
		const char *payload),
	void *io)
{
	JSON_Value *v = json_parse_string(payload);
	JSON_Object *o;
	char nonce[128], session[128], cmdline[256];
	struct agent_entry *e;
	double pid = 0, ppid = 0;

	if (v == NULL)
		return NULL;
	o = json_value_get_object(v);
	jstr(o, "nonce", nonce, sizeof(nonce));
	jstr(o, "session_token", session, sizeof(session));
	jstr(o, "cmdline", cmdline, sizeof(cmdline));
	if (o != NULL) {
		pid = json_object_get_number(o, "pid");
		ppid = json_object_get_number(o, "ppid");
	}
	/* no nonce in HELLO: the spectator seat */
	st->spectator = daemon_nonce != NULL &&
		strcmp(nonce, daemon_nonce) != 0;
	e = retraced_registry_hello(reg, NULL, (long)pid, (long)ppid,
		session, cmdline);
	json_value_free(v);
	if (e == NULL)
		return NULL;
	snprintf(st->agent_id, sizeof(st->agent_id), "%s", e->id);
	st->helloed = 1;
	e->spectator = st->spectator;
	daemon_frame_welcome(st, ctl, write_frame, io, jr);
	return e;
}
