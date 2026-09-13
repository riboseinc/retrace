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
		/* Control-plane audit (the reap cycle's lesson,
		 * phase-3's shape): a journaled ORDER, LAUNCH, or
		 * DEPARTURE is an audit decision -- a live auditor
		 * must not wait for an unrelated flush. An exit
		 * record rides NO traffic (the child is gone), so
		 * without this it sat buffered forever on an idle
		 * daemon.
		 */
		"\"name\":\"retrace.ctl.",
		/* Live drift grading (03 P1): a kernel-obs delta is
		 * the daemon's own heartbeat-grade of sub-libc
		 * escapes -- audit-class, never buffered away.
		 */
		"\"name\":\"retrace.drift.",
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
	snprintf(j->base, sizeof(j->base), "%s", path);
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

void retraced_journal_flush(struct retraced_journal *j)
{
	if (j != NULL && j->f != NULL)
		fflush(j->f);
}

/* ---- rotation + retention (TODO.impl/10) ------------------------------- */

void retraced_journal_set_rotation(struct retraced_journal *j,
	uint64_t rotate_bytes, long rotate_seconds,
	uint64_t budget_bytes)
{
	j->rotate_bytes = rotate_bytes;
	j->rotate_seconds = rotate_seconds;
	j->budget_bytes = budget_bytes;
	j->seg_bytes = 0;
	j->last_rotate_ts = (long) time(NULL);

	/* resolve the live segment: the highest existing number,
	 * else 0 (a fresh series). The lazy writer opens it on the
	 * first append; replay reads the whole series first.
	 */
	if (rotate_bytes != 0 || rotate_seconds != 0) {
		int seq = 0;

		while (seq < 9999) {
			char p[600];

			retraced_journal_segment_path(j->base, seq + 1,
				p, sizeof(p));
			if (!retraced_journal_segment_exists(p, NULL))
				break;
			seq++;
		}
		j->segment_seq = seq;
		retraced_journal_segment_path(j->base, seq,
			j->path, sizeof(j->path));
	}
}

void retraced_journal_segment_path(const char *base, int seq,
	char *out, size_t cap)
{
	snprintf(out, cap, "%s.%04d", base, seq);
}

int retraced_journal_segment_exists(const char *path,
	long long *size_out)
{
	FILE *f = fopen(path, "rb");

	if (f == NULL)
		return 0;
	if (size_out != NULL) {
		*size_out = 0;
		if (fseek(f, 0, SEEK_END) == 0)
			*size_out = ftell(f);
	}
	fclose(f);
	return 1;
}

int retraced_journal_segment_range(const char *base,
	int *lo_out, int *hi_out)
{
	int hi = -1;
	int lo = -1;
	int seq;

	for (seq = 9999; seq >= 0 && hi < 0; seq--) {
		char p[600];

		retraced_journal_segment_path(base, seq, p,
			sizeof(p));
		if (retraced_journal_segment_exists(p, NULL))
			hi = seq;
	}
	if (hi < 0)
		return -1;
	lo = retraced_journal_segment_lo(base, hi);
	if (lo < 0)
		return -1;
	*lo_out = lo;
	*hi_out = hi;
	return 0;
}

int retraced_journal_segment_lo(const char *base, int hi)
{
	int seq;

	for (seq = 0; seq <= hi; seq++) {
		char p[600];

		retraced_journal_segment_path(base, seq, p, sizeof(p));
		if (retraced_journal_segment_exists(p, NULL))
			return seq;
	}
	return -1;
}

/* Retention: prune OLDEST segments until the pruned set fits
 * the budget. The live segment is never pruned; each prune is
 * a chained record in the live segment (auditable gaps).
 */
static void journal_retain(struct retraced_journal *j, long ts)
{
	if (j->budget_bytes == 0)
		return;

	for (;;) {
		int lo = retraced_journal_segment_lo(j->base,
			j->segment_seq);
		uint64_t sum = 0;
		char p[600];
		char ev[256];
		long long sz;
		int seq;

		if (lo < 0 || lo >= j->segment_seq)
			return;	/* nothing prunable left */

		/* the budget caps the SUM of the closed segments */
		for (seq = lo; seq < j->segment_seq; seq++) {
			long long sz2;

			retraced_journal_segment_path(j->base, seq,
				p, sizeof(p));
			if (retraced_journal_segment_exists(p, &sz2))
				sum += (uint64_t) sz2;
		}
		if (sum <= j->budget_bytes)
			return;	/* the series fits: keep it all */

		/* prune the OLDEST; the record is a chained event */
		retraced_journal_segment_path(j->base, lo, p,
			sizeof(p));
		if (!retraced_journal_segment_exists(p, &sz))
			return;
		if (remove(p) != 0)
			return;
		snprintf(ev, sizeof(ev),
			"{\"name\":\"retrace.journal.segment_pruned\","
			"\"segment\":\"%s\",\"bytes\":%lld}",
			p, sz);
		(void)retraced_journal_event(j, ts, "daemon", 0, ev);
	}
}

/* Close the live segment (chained close marker with its head),
 * run retention, open the next with a genesis link carrying
 * the predecessor's head -- the chain continues across files.
 */
static void journal_rotate(struct retraced_journal *j, long ts)
{
	char closed[600];
	char opened[600];
	char ev[512];
	uint64_t head = j->prev_hash;
	int old_seq = j->segment_seq;

	j->rotating = 1;
	retraced_journal_segment_path(j->base, old_seq, closed,
		sizeof(closed));

	(void)snprintf(ev, sizeof(ev),
		"{\"name\":\"retrace.journal.segment_closed\","
		"\"segment\":\"%s\",\"head\":\"%016llx\","
		"\"lines\":%llu}",
		closed, (unsigned long long) head,
		(unsigned long long) j->lines);
	(void)retraced_journal_event(j, ts, "daemon", 0, ev);

	j->segment_seq++;
	retraced_journal_segment_path(j->base, j->segment_seq,
		j->path, sizeof(j->path));
	retraced_journal_segment_path(j->base, j->segment_seq,
		opened, sizeof(opened));

	if (j->f != NULL) {
		fflush(j->f);
		fclose(j->f);
		j->f = NULL;
	}
	j->seg_bytes = 0;
	j->lines = 0;
	j->last_rotate_ts = ts;

	journal_retain(j, ts);

	(void)snprintf(ev, sizeof(ev),
		"{\"name\":\"retrace.journal.segment_opened\","
		"\"segment\":\"%s\",\"prev_segment\":\"%s\","
		"\"prev_head\":\"%016llx\"}",
		opened, closed, (unsigned long long) head);
	(void)retraced_journal_event(j, ts, "daemon", 0, ev);
	j->rotating = 0;
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
	j->seg_bytes += (uint64_t) n;

	if (!j->rotating &&
	    (j->rotate_bytes != 0 || j->rotate_seconds != 0)) {
		int rotate = 0;

		if (j->rotate_bytes != 0 && j->seg_bytes >= j->rotate_bytes)
			rotate = 1;
		if (j->rotate_seconds != 0 &&
		    ts - j->last_rotate_ts >= j->rotate_seconds)
			rotate = 1;
		if (rotate)
			journal_rotate(j, ts);
	}
	return 0;
}

/*
 * Verify + rebuild. Each stored line carries the PREVIOUS
 * line's hash; we recompute every link in order. A torn tail
 * (partial last line, no newline) stops the replay cleanly.
 */
int retraced_journal_tail(struct retraced_journal *j, size_t last_n,
	void (*sink)(const char *line, void *user), void *user,
	long *chain_out)
{
	/*
	 * one forward pass, same verify arithmetic as replay
	 * (replay's code path stays untouched: boot-time state
	 * rebuild is its own contract). The retained ring holds
	 * the final last_n records; everything earlier is
	 * verified and dropped.
	 */
	FILE *f = fopen(j->path, "r");
	char **ring;
	uint64_t prev = 0;
	size_t kept = 0;
	size_t ring_i = 0;
	uint64_t lineno = 0;
	size_t i;

	if (chain_out != NULL)
		*chain_out = 0;
	if (f == NULL)
		return -1;
	if (last_n == 0)
		last_n = 1;
	ring = (char **)calloc(last_n, sizeof(*ring));
	if (ring == NULL) {
		fclose(f);
		return -1;
	}

	{
		char line[2048];

		while (fgets(line, sizeof(line), f) != NULL) {
			size_t len = strlen(line);
			uint64_t link;
			uint64_t stored_prev;
			JSON_Value *v;
			JSON_Object *o;

			lineno++;
			if (len == 0 || line[len - 1] != '\n')
				break;	/* torn tail: stop cleanly */
			v = json_parse_string(line);
			if (v == NULL)
				break;	/* corrupt line: torn */
			o = json_value_get_object(v);
			stored_prev = (uint64_t)strtoull(
				json_object_get_string(o, "prev"),
				NULL, 16);
			json_value_free(v);
			if (stored_prev != prev) {
				/* the verdict is the payload: report
				 * where the chain broke
				 */
				if (chain_out != NULL)
					*chain_out = (long)lineno;
				break;
			}
			link = fnv1a(line,
				prev ^ 0x9e3779b97f4a7c15ULL);

			free(ring[ring_i]);
			ring[ring_i] = strdup(line);
			ring_i = (ring_i + 1) % last_n;
			if (kept < last_n)
				kept++;
			prev = link;
		}
	}
	fclose(f);

	for (i = 0; i < kept; i++) {
		size_t idx = (kept < last_n) ? i :
			(ring_i + i) % last_n;

		if (ring[idx] != NULL && sink != NULL)
			sink(ring[idx], user);
		free(ring[idx]);
	}
	free(ring);
	return (int)kept;
}

/*
 * One file of the replay walk. `prev` is the chain head this
 * file must continue from (0 for a series's first segment).
 * Tolerates a torn tail only when `last` -- a torn MIDDLE
 * segment is corruption (fail-closed). Returns the head, or
 * (uint64_t)-1 on a broken chain / unreadable middle file.
 */
static uint64_t journal_replay_file(struct retraced_journal *j,
	struct retraced_registry *r, const char *path,
	uint64_t prev, int last)
{
	FILE *f = fopen(path, "r");
	char line[2048];
	uint64_t lineno = 0;
	int saw_torn = 0;

	if (f == NULL) {
		if (last)
			return prev;
		j->chain_broken_at = 0;
		return (uint64_t) -1;
	}

	while (fgets(line, sizeof(line), f) != NULL) {
		size_t len = strlen(line);
		uint64_t link;
		uint64_t stored_prev;
		JSON_Value *v;
		JSON_Object *o;
		const char *agent;
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
		{
			const char *prev_str = json_object_get_string(o,
				"prev");

			/* a line without its chain link is malformed
			 * (tampering, not a torn tail): broken
			 */
			if (prev_str == NULL) {
				j->chain_broken_at = (int)lineno;
				json_value_free(v);
				fclose(f);
				return (uint64_t) -1;
			}
			stored_prev = (uint64_t)strtoull(
				prev_str, NULL, 16);
		}
		if (stored_prev != prev) {
			j->chain_broken_at = (int)lineno;
			json_value_free(v);
			fclose(f);
			return (uint64_t) -1;
		}
		link = fnv1a(line,
			prev ^ 0x9e3779b97f4a7c15ULL);

		agent = json_object_get_string(o, "agent");
		seq = json_object_get_number(o, "seq");
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
	j->replay_ok += lineno - (saw_torn ? 1 : 0);
	if (last)
		j->lines = lineno - (saw_torn ? 1 : 0);
	return prev;
}

int retraced_journal_replay(struct retraced_journal *j,
	struct retraced_registry *r)
{
	j->replay_ok = 0;
	j->replay_events = 0;
	j->chain_broken_at = -1;
	j->clean_close = 0;

	/* the rotation series: oldest -> live, one chain across
	 * all segments (each genesis carries its predecessor's
	 * head, so continuity is verified, not assumed)
	 */
	if (j->rotate_bytes != 0 || j->rotate_seconds != 0) {
		int lo = retraced_journal_segment_lo(j->base,
			j->segment_seq);
		uint64_t prev = 0;
		int seq;

		if (lo < 0)
			return 0;	/* fresh series */
		for (seq = lo; seq <= j->segment_seq; seq++) {
			char p[600];

			retraced_journal_segment_path(j->base, seq,
				p, sizeof(p));
			prev = journal_replay_file(j, r, p, prev,
				seq == j->segment_seq);
			if (prev == (uint64_t) -1)
				return -1;
		}
		j->prev_hash = prev;
		return 0;
	}

	/* the single-file journal: a missing file is a fresh boot
	 * (the daemon's "no prior journal" path)
	 */
	if (!retraced_journal_segment_exists(j->path, NULL))
		return -1;

	{
		uint64_t prev = journal_replay_file(j, r, j->path, 0, 1);

		if (prev == (uint64_t) -1)
			return -1;
		j->prev_hash = prev;
		return 0;
	}
}

uint64_t retraced_journal_line_hash(const char *line,
	uint64_t prev)
{
	return fnv1a(line, prev ^ 0x9e3779b97f4a7c15ULL);
}

int retraced_journal_chain_file(const char *path, size_t stop_at,
	uint64_t *head_out, size_t *lines_out, size_t *broken_at)
{
	return retraced_journal_chain_file_from(path, 0, stop_at,
		head_out, lines_out, broken_at);
}

/*
 * The segment-aware variant (TODO.impl/10): verify one segment
 * of a rotated series, starting from the PREDECESSOR's final
 * head -- the chain continues across files, so a non-first
 * segment's genesis link is that head, not zero.
 */
int retraced_journal_chain_file_from(const char *path,
	uint64_t start_prev, size_t stop_at,
	uint64_t *head_out, size_t *lines_out, size_t *broken_at)
{
	FILE *f = fopen(path, "r");
	char line[2048];
	uint64_t prev = start_prev;
	size_t lines = 0;

	*broken_at = 0;
	if (head_out != NULL)
		*head_out = 0;
	if (lines_out != NULL)
		*lines_out = 0;
	if (f == NULL)
		return -1;
	while (stop_at == 0 || lines < stop_at) {
		if (fgets(line, sizeof(line), f) == NULL)
			break;
		char prev_hex[32];
		size_t n = strlen(line);
		uint64_t link;

		lines++;
		/* each line carries the PREVIOUS line's link; verify
		 * it, then compute this line's (replay's arithmetic)
		 */
		{
			const char *p = strstr(line, "\"prev\":\"");

			if (p == NULL) {
				*broken_at = lines;
				break;
			}
			snprintf(prev_hex, sizeof(prev_hex), "%s", p + 8);
			prev_hex[16] = '\0';
			if (strtoull(prev_hex, NULL, 16) != prev) {
				*broken_at = lines;
				break;
			}
		}
		(void)n;
		/* the writer hashes the line WITH its newline
		 * (journal_event snprintfs the full line first) --
		 * tail does the same; hash exactly what is stored
		 */
		link = fnv1a(line, prev ^ 0x9e3779b97f4a7c15ULL);
		prev = link;
	}
	fclose(f);
	if (lines_out != NULL)
		*lines_out = lines;
	if (head_out != NULL)
		*head_out = prev;
	return *broken_at == 0 ? 0 : -1;
}
