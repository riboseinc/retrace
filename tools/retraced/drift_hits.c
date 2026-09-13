/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * The drift-hit matcher (TODO.impl/18). See drift_hits.h.
 *
 * Shapes: two bounded rings (claims, hit-dedup), time-windowed
 * on both sides -- a claim ages out (the lane's evidence is a
 * stream, not a database) and a repeat hit is folded while the
 * dedup window holds (a hot escaping loop names itself once,
 * not per iteration; the sweep-scale cadence stays readable).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "drift_hits.h"

#define CLAIM_RING 128
#define HIT_RING 64
#define CLAIM_WINDOW_S 5
#define HIT_DEDUP_S 15
#define OP_MAX 32
#define PATH_MAX_LEN 192

struct claim {
	long pid;
	char op[OP_MAX];
	char path[PATH_MAX_LEN];
	time_t ts;
};

struct hit_key {
	long pid;
	char op[OP_MAX];
	char path[PATH_MAX_LEN];
	time_t ts;
};

static struct claim g_claims[CLAIM_RING];
static size_t g_claims_next;
static struct hit_key g_hits[HIT_RING];
static size_t g_hits_next;

static void key_copy(struct claim *c, long pid, const char *op,
	const char *path, time_t now)
{
	c->pid = pid;
	snprintf(c->op, sizeof(c->op), "%s", op != NULL ? op : "");
	snprintf(c->path, sizeof(c->path), "%s",
		path != NULL ? path : "");
	c->ts = now;
}

static int same_key(const struct claim *c, long pid,
	const char *op, const char *path)
{
	if (strcmp(c->op, op != NULL ? op : "") != 0)
		return 0;
	if (strcmp(c->path, path != NULL ? path : "") != 0)
		return 0;
	/* pid is a REFINEMENT, not a filter: a lane that carries
	 * no pid (<=0) matches the other side's any pid
	 */
	if (pid > 0 && c->pid > 0 && pid != c->pid)
		return 0;
	return 1;
}

static int claim_covers(long pid, const char *op,
	const char *path, time_t now)
{
	size_t i;

	for (i = 0; i < CLAIM_RING; i++) {
		struct claim *c = &g_claims[i];

		if (c->ts == 0)
			continue;
		if (now - c->ts > CLAIM_WINDOW_S)
			continue;	/* aged out */
		if (same_key(c, pid, op, path))
			return 1;
	}
	return 0;
}

static int hit_recent(long pid, const char *op, const char *path,
	time_t now)
{
	size_t i;

	for (i = 0; i < HIT_RING; i++) {
		struct hit_key *h = &g_hits[i];

		if (h->ts == 0)
			continue;
		if (now - h->ts > HIT_DEDUP_S)
			continue;
		if (same_key((struct claim *)h, pid, op, path))
			return 1;
	}
	return 0;
}

void retrace_drift_claim(long pid, const char *op,
	const char *path)
{
	time_t now = time(NULL);

	key_copy(&g_claims[g_claims_next], pid, op, path, now);
	g_claims_next = (g_claims_next + 1) % CLAIM_RING;
}

int retrace_drift_observe(long pid, const char *op,
	const char *path)
{
	time_t now = time(NULL);
	struct hit_key *h;

	if (path == NULL || path[0] == '\0')
		return 0;	/* names nothing; counts already ride */
	if (claim_covers(pid, op, path, now))
		return 0;
	if (hit_recent(pid, op, path, now))
		return 0;	/* named already within the window */
	h = &g_hits[g_hits_next];
	key_copy((struct claim *)h, pid, op, path, now);
	g_hits_next = (g_hits_next + 1) % HIT_RING;
	return 1;
}

void retrace_drift_hits_reset(void)
{
	memset(g_claims, 0, sizeof(g_claims));
	memset(g_hits, 0, sizeof(g_hits));
	g_claims_next = 0;
	g_hits_next = 0;
}
