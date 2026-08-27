/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "journal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "parson.h"

/* FNV-1a 64 -- cheap, inline, enough for tamper EVIDENCE
 * (signatures come with plan 05).
 */
static uint64_t fnv1a(const char *s, uint64_t h)
{
	for (; *s != '\0'; s++) {
		h ^= (unsigned char)*s;
		h *= 0x01000193ULL;
	}
	return h & 0xffffffffffffffffULL;
}

/*
 * Durability classes (the audit contract): control-plane
 * records are flushed the moment they are written -- an
 * auth decision, policy push, or session mint surviving a
 * crash is the compliance story. Routine agent telemetry is
 * buffered; on an unclean shutdown the buffered tail is lost
 * and the NEXT boot journals retrace.journal.unclean so the
 * gap is recorded, never silent (the same doctrine as the
 * agent's drop signaling).
 */
static int payload_is_durable(const char *payload)
{
	static const char *const classes[] = {
		"\"name\":\"retrace.auth.",
		"\"name\":\"retrace.policy.",
		"\"name\":\"retrace.session.",
		"\"name\":\"retrace.journal.",
		/* POLICY_ACK records carry no name field -- they ARE
		 * policy decisions (applied or refused) and belong
		 * on the durable side (the phase-3 lesson: a
		 * buffered refusal ack is invisible to a live
		 * auditor until an unrelated flush)
		 */
		"\"applied\":",
	};
	size_t i;

	if (payload == NULL)
		return 0;
	for (i = 0; i < sizeof(classes) / sizeof(classes[0]); i++) {
		if (strstr(payload, classes[i]) != NULL)
			return 1;
	}
	return 0;
}

int retraced_journal_open(struct retraced_journal *j,
	const char *path)
{
	memset(j, 0, sizeof(*j));
	snprintf(j->path, sizeof(j->path), "%s", path);
	j->chain_broken_at = -1;
	j->f = NULL;
	j->clean_close = 0;
	/* append mode: history is immutable; the chain head
	 * resumes from replay
	 */
	return 0;
}

void retraced_journal_close(struct retraced_journal *j)
{
	/* the close marker is itself a chained, flushed record:
	 * its absence on the next boot means the buffered tail
	 * was lost to an unclean shutdown -- and that gap gets
	 * its own journal record (never silent)
	 */
	(void)retraced_journal_event(j, (long)time(NULL), "daemon",
		0, "{\"name\":\"retrace.journal.closed\"}");
	if (j->f != NULL) {
		fflush(j->f);
		fclose(j->f);
		j->f = NULL;
	}
	j->clean_close = 1;
}

/*
 * The writer deepening (the audit story's throughput floor):
 * ONE fopen for the daemon's lifetime, not one per event --
 * an open(2)+close(2) per journal line bought nothing, since
 * every line was already safely in the OS page cache. The
 * FILE* is opened lazily on first append so a read-only boot
 * (replay-only, refused start) never creates the file.
 */
static FILE *journal_writer(struct retraced_journal *j)
{
	if (j->f == NULL)
		j->f = fopen(j->path, "a");
	return j->f;
}

int retraced_journal_event(struct retraced_journal *j,
	long ts, const char *agent_id, uint64_t seq,
	const char *payload)
{
	FILE *f;
	char line[2048];
	uint64_t link;
	int n;

	n = snprintf(line, sizeof(line),
		"{\"ts\":%ld,\"agent\":\"%s\",\"seq\":%llu,\"prev\":\"%016llx\",\"ev\":%s}\n",
		ts, agent_id, (unsigned long long)seq,
		(unsigned long long)j->prev_hash,
		payload != NULL ? payload : "{}");
	if (n <= 0 || (size_t)n >= sizeof(line))
		return -1;

	link = fnv1a(line, j->prev_hash ^ 0x9e3779b97f4a7c15ULL);

	f = journal_writer(j);
	if (f == NULL)
		return -1;
	if (fputs(line, f) == EOF)
		return -1;
	if (payload_is_durable(payload))
		fflush(f);

	j->prev_hash = link;
	j->lines++;
	return 0;
}

/*
 * Verify + rebuild. Each stored line carries the PREVIOUS
 * line's hash; we recompute every link in order. A torn tail
 * (partial last line, no newline) stops the replay cleanly.
 */
int retraced_journal_replay(struct retraced_journal *j,
	struct retraced_registry *r)
{
	FILE *f = fopen(j->path, "r");
	char line[2048];
	uint64_t prev = 0;
	uint64_t lineno = 0;
	int saw_torn = 0;

	j->replay_ok = 0;
	j->replay_events = 0;
	j->chain_broken_at = -1;
	j->clean_close = 0;
	if (f == NULL)
		return -1;

	while (fgets(line, sizeof(line), f) != NULL) {
		size_t len = strlen(line);
		uint64_t link;
		uint64_t stored_prev;
		JSON_Value *v;
		JSON_Object *o;
		const char *agent;
		const char *ev;
		double seq;

		lineno++;
		if (len == 0 || line[len - 1] != '\n') {
			/* torn tail: normal after a crash */
			saw_torn = 1;
			break;
		}
		/* only a marker as the LAST complete line counts */
		j->clean_close = strstr(line,
			"\"name\":\"retrace.journal.closed\"") != NULL;
		v = json_parse_string(line);
		if (v == NULL) {
			saw_torn = 1; /* corrupt tail: treat as torn */
			break;
		}
		o = json_value_get_object(v);
		stored_prev = (uint64_t)strtoull(
			json_object_get_string(o, "prev"), NULL, 16);
		if (stored_prev != prev) {
			j->chain_broken_at = (int)lineno;
			json_value_free(v);
			fclose(f);
			return -1;
		}
		link = fnv1a(line,
			prev ^ 0x9e3779b97f4a7c15ULL);

		agent = json_object_get_string(o, "agent");
		ev = json_object_get_string(o, "ev");
		seq = json_object_get_number(o, "seq");
		(void)ev;
		if (agent != NULL) {
			struct agent_entry *e =
				retraced_registry_find(r, agent);

			if (e == NULL)
				e = retraced_registry_hello(r, agent,
					(long)json_object_get_number(o,
						"pid"),
					0, "", "");
			if (e != NULL) {
				e->last_seq = (uint64_t)seq;
				e->state = AGENT_LIVE;
			}
		}
		json_value_free(v);
		j->replay_events++;
		prev = link;
	}
	fclose(f);
	j->replay_ok = lineno - (saw_torn ? 1 : 0);
	j->prev_hash = prev;
	j->lines = lineno - (saw_torn ? 1 : 0);
	(void)saw_torn;
	return 0;
}
