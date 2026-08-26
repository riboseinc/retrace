/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */
#ifndef RETRACE_TOOLS_REGISTRY_H_
#define RETRACE_TOOLS_REGISTRY_H_

#include <stddef.h>
#include <stdint.h>

/*
 * The retraced process registry (TODO.supervisor/02).
 *
 * Pure state: agent entries keyed by agent_id (daemon-minted
 * ids; the HELLO carries pid/boot_id, the daemon mints
 * "<boot>.<pid>.<counter>"). No sockets, no I/O -- the event
 * loop owns transport; this module owns the answer to "what is
 * running". Bounded (RETRACED_REGISTRY_MAX); a full registry
 * refuses new agents (fail-closed authority, plan 08).
 */

#define RETRACED_REGISTRY_MAX 1024
#define RETRACED_AGENT_ID_MAX 96
#define RETRACED_SESSION_MAX 65
#define RETRACED_CMDLINE_MAX 512

enum agent_state {
	AGENT_LIVE = 0,
	AGENT_STALE = 1,
	AGENT_GONE = 2,
};

struct agent_entry {
	char id[RETRACED_AGENT_ID_MAX];
	char session[RETRACED_SESSION_MAX];
	char parent_id[RETRACED_AGENT_ID_MAX];
	char cmdline[RETRACED_CMDLINE_MAX];
	long pid;
	long ppid;
	int parent_hole;	/* ppid known, no traced agent there */
	int spectator;		/* nonceless HELLO: evidence only */
	uint64_t policy_epoch;
	uint64_t last_seq;
	long last_hb_ms;
	enum agent_state state;
};

struct retraced_registry {
	struct agent_entry agents[RETRACED_REGISTRY_MAX];
	size_t count;
	uint64_t id_counter;
};

void retraced_registry_init(struct retraced_registry *r);

/*
 * Mint an agent id from the HELLO's pid + boot_id hash and
 * register the entry. Returns the new entry, or NULL when the
 * registry is full. Re-HELLO with a known id re-binds it
 * (agent reconnect: plan 03's fail-open liveness).
 */
struct agent_entry *retraced_registry_hello(
	struct retraced_registry *r, const char *agent_id_in,
	long pid, long ppid, const char *session,
	const char *cmdline);

struct agent_entry *retraced_registry_find(
	struct retraced_registry *r, const char *agent_id);

/*
 * Mint a fresh 128-bit session token (hex). The daemon mints at
 * first HELLO; controller-supplied tokens (plan 08) arrive the
 * same way through the HELLO payload.
 */
void retraced_registry_mint_session(
	char out[RETRACED_SESSION_MAX]);

/*
 * Stitch the tree edge: resolve e->ppid against registered
 * agents. Returns the parent entry, or NULL when the ppid is not
 * a traced agent -- recorded as a HOLE (an untraced intermediate:
 * a shell, an env-scrubbed exec hop; the root's own parent is
 * that too, honestly). Idempotent per entry.
 */
struct agent_entry *retraced_registry_link_parent(
	struct retraced_registry *r, struct agent_entry *e);

/* Mark gone (BYE); absent id is a no-op (idempotent). */
void retraced_registry_bye(struct retraced_registry *r,
	const char *agent_id);

/*
 * Heartbeat: update liveness + the event sequence watermark
 * (gap detection is the caller's -- the ring-pull trigger).
 * Returns 0 ok, -1 unknown agent.
 */
int retraced_registry_heartbeat(struct retraced_registry *r,
	const char *agent_id, uint64_t seq, long now_ms);

/* Mark entries stale whose heartbeat is older than timeout_ms. */
size_t retraced_registry_sweep(struct retraced_registry *r,
	long now_ms, long timeout_ms);

/* Serialize (owned JSON value; caller frees) for the snapshot. */
struct json_value_t; /* parson fwd */
struct json_value_t *retraced_registry_to_json(
	const struct retraced_registry *r);

#endif /* RETRACE_TOOLS_REGISTRY_H_ */
