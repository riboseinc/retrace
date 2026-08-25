/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "registry.h"

#include <stdio.h>
#include <string.h>

#include "parson.h"

void retraced_registry_init(struct retraced_registry *r)
{
	memset(r, 0, sizeof(*r));
}

struct agent_entry *retraced_registry_find(
	struct retraced_registry *r, const char *agent_id)
{
	size_t i;

	if (agent_id == NULL)
		return NULL;
	for (i = 0; i < r->count; i++) {
		if (strcmp(r->agents[i].id, agent_id) == 0)
			return &r->agents[i];
	}
	return NULL;
}

struct agent_entry *retraced_registry_hello(
	struct retraced_registry *r, const char *agent_id_in,
	long pid, long ppid, const char *session,
	const char *cmdline)
{
	struct agent_entry *e;
	char minted[RETRACED_AGENT_ID_MAX];

	if (agent_id_in == NULL || agent_id_in[0] == '\0') {
		snprintf(minted, sizeof(minted), "boot.%ld.%lu",
			pid, (unsigned long)(++r->id_counter));
		agent_id_in = minted;
	}

	/* Reconnect: re-bind the existing entry (plan 03's
	 * fail-open liveness -- the registry entry survives the
	 * connection it arrived on).
	 */
	e = retraced_registry_find(r, agent_id_in);
	if (e != NULL) {
		e->state = AGENT_LIVE;
		return e;
	}
	if (r->count >= RETRACED_REGISTRY_MAX)
		return NULL;
	e = &r->agents[r->count++];
	snprintf(e->id, sizeof(e->id), "%s", agent_id_in);
	e->pid = pid;
	e->ppid = ppid;
	snprintf(e->session, sizeof(e->session), "%s",
		session != NULL ? session : "");
	snprintf(e->cmdline, sizeof(e->cmdline), "%s",
		cmdline != NULL ? cmdline : "");
	e->policy_epoch = 0;
	e->last_seq = 0;
	e->last_hb_ms = 0;
	e->state = AGENT_LIVE;
	return e;
}

void retraced_registry_bye(struct retraced_registry *r,
	const char *agent_id)
{
	struct agent_entry *e = retraced_registry_find(r, agent_id);

	if (e != NULL)
		e->state = AGENT_GONE;
}

int retraced_registry_heartbeat(struct retraced_registry *r,
	const char *agent_id, uint64_t seq, long now_ms)
{
	struct agent_entry *e = retraced_registry_find(r, agent_id);

	if (e == NULL)
		return -1;
	e->last_hb_ms = now_ms;
	e->last_seq = seq;
	e->state = AGENT_LIVE;
	return 0;
}

size_t retraced_registry_sweep(struct retraced_registry *r,
	long now_ms, long timeout_ms)
{
	size_t i;
	size_t stale = 0;

	for (i = 0; i < r->count; i++) {
		if (r->agents[i].state == AGENT_LIVE &&
		    r->agents[i].last_hb_ms > 0 &&
		    now_ms - r->agents[i].last_hb_ms > timeout_ms) {
			r->agents[i].state = AGENT_STALE;
			stale++;
		}
	}
	return stale;
}

struct json_value_t *retraced_registry_to_json(
	const struct retraced_registry *r)
{
	JSON_Value *root = json_value_init_object();
	JSON_Value *agents_val = json_value_init_array();
	JSON_Array *arr = json_value_get_array(agents_val);
	size_t i;

	json_object_set_number(json_value_get_object(root), "count",
		(double)r->count);
	json_object_set_value(json_value_get_object(root), "agents",
		agents_val);
	for (i = 0; i < r->count; i++) {
		const struct agent_entry *e = &r->agents[i];
		JSON_Value *v = json_value_init_object();
		JSON_Object *o = json_value_get_object(v);

		json_object_set_string(o, "id", e->id);
		json_object_set_string(o, "session", e->session);
		json_object_set_string(o, "cmdline", e->cmdline);
		json_object_set_number(o, "pid", (double)e->pid);
		json_object_set_number(o, "ppid", (double)e->ppid);
		json_object_set_number(o, "policy_epoch",
			(double)e->policy_epoch);
		json_object_set_number(o, "last_seq",
			(double)e->last_seq);
		json_object_set_number(o, "last_hb_ms",
			(double)e->last_hb_ms);
		json_object_set_string(o, "state",
			e->state == AGENT_LIVE ? "live" :
			e->state == AGENT_STALE ? "stale" : "gone");
		json_array_append_value(arr, v);
	}
	return root;
}
