/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * retrace-campaign (TODO.impl/09): the farm arm. A manifest
 * expands to a matrix of runs (samples x policies x repeats);
 * a bounded worker pool drives each cell through the daemon's
 * PUBLIC surface -- policy-push, then spawn -- and waits for
 * the run's departure by polling the journal query arm for
 * its pid's retrace.ctl.exit record. Every launch and every
 * departure lands in the daemon's journal (the reap
 * doctrine); the campaign only reads it back.
 *
 * The tool is a pure client: no daemon code is linked or
 * modified. retrace-ctl is invoked as a subprocess per verb
 * (a farm's cost is the detonations, not the control plane).
 */

#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>

#include "parson.h"

#ifndef CAMPAIGN_CTL
#define CAMPAIGN_CTL "retrace-ctl"
#endif

/* Append `arg` to the command buffer, single-quoted for the
 * shell ('...' with '\'' escapes): paths and filter
 * expressions contain spaces, quotes, and metacharacters, and
 * an unquoted splice is both broken parsing and an injection
 * vector through manifest values.
 */
static size_t
cmd_arg(char *cmd, size_t cap, size_t o, const char *arg)
{
	o += (size_t)snprintf(cmd + o, cap - o, " '");
	for (; *arg != '\0' && o + 4 < cap; arg++) {
		if (*arg == '\'')
			o += (size_t)snprintf(cmd + o, cap - o,
				"'\\''");
		else
			cmd[o++] = *arg;
	}
	o += (size_t)snprintf(cmd + o, cap - o, "'");
	return o;
}

struct run_result {
	int cell;
	long pid;
	int done;		/* exit record observed */
	int signaled;
	int code;
	int push_ok;
	int spawn_ok;
	double elapsed;
};

struct pool {
	struct campaign *c;
	struct run_result *results;
	size_t next_cell;	/* guarded by the mutex */
	pthread_mutex_t mu;
};

/* ---- the ctl subprocess ------------------------------------------------- */

static int
ctl_run(char *out, size_t out_cap, const struct campaign *c,
	char *const extra[])
{
	char cmd[2048];
	size_t o = 0;
	int rc;
	int k;

	o += (size_t)snprintf(cmd + o, sizeof(cmd) - o, "%s",
		CAMPAIGN_CTL);
	if (c->ctl_sock[0] != '\0') {
		o += (size_t)snprintf(cmd + o, sizeof(cmd) - o,
			" --sock");
		o = cmd_arg(cmd, sizeof(cmd), o, c->ctl_sock);
	}
	for (k = 0; extra[k] != NULL && o + 4 < sizeof(cmd); k++)
		o = cmd_arg(cmd, sizeof(cmd), o, extra[k]);

	if (out == NULL)
		return system(cmd) == 0 ? 0 : -1;

	{
		FILE *p = popen(cmd, "r");

		if (p == NULL)
			return -1;
		out[0] = '\0';
		while (fgets(out, (int)out_cap, p) != NULL) {
			/* keep the LAST line (the reply) */
		}
		rc = pclose(p);
	}
	return rc == 0 ? 0 : -1;
}

/* Poll the journal query arm for the run's exit record. */
static int
await_exit(const struct campaign *c, long pid, int timeout_sec,
	int *signaled, int *code)
{
	char expr[128];
	char *const qv[] = {
		(char *)"events", (char *)"--journal",
		c->journal_base, (char *)"--query", expr, NULL
	};
	time_t start = time(NULL);

	snprintf(expr, sizeof(expr),
		"name == \"retrace.ctl.exit\" and pid == %ld", pid);

	while (time(NULL) - start < timeout_sec) {
		char out[2048];

		if (ctl_run(out, sizeof(out), c, qv) == 0 &&
		    out[0] == '{') {
			JSON_Value *v = json_parse_string(out);

			if (v != NULL) {
				JSON_Object *o = json_value_get_object(v);
				JSON_Object *ev = json_object_get_object(o,
					"ev");

				if (ev != NULL &&
				    json_object_has_value(ev, "how")) {
					const char *how =
						json_object_get_string(ev,
							"how");

					*signaled = how != NULL &&
						strcmp(how,
							"signaled") == 0;
					*code = (int)
						json_object_get_number(ev,
							"code");
					json_value_free(v);
					return 0;
				}
			}
			json_value_free(v);
		}
		struct timespec ts = { 1, 0 };

		nanosleep(&ts, NULL);
	}
	return -1;		/* timeout */
}

/* ---- one cell ----------------------------------------------------------- */

static void
run_cell(struct campaign *c, int cell_idx, struct run_result *r)
{
	struct campaign_cell *cell = &c->cells[cell_idx];
	struct campaign_sample *s = &c->samples[cell->sample];
	struct campaign_policy *p = &c->policies[cell->policy];
	int k;
	time_t t0 = time(NULL);

	memset(r, 0, sizeof(*r));
	r->cell = cell_idx;

	/* 1. policy: push the cell's policy file */
	{
		char *const pv[] = { (char *)"policy-push",
			p->path, NULL };

		r->push_ok = ctl_run(NULL, 0, c, pv) == 0;
	}
	if (!r->push_ok)
		return;

	/* 2. spawn: the launch arm (one ctl invocation) */
	{
		char out[2048];
		char cmd[2048];
		size_t co = 0;
		FILE *proc;

		co += (size_t)snprintf(cmd + co, sizeof(cmd) - co,
			"%s", CAMPAIGN_CTL);
		if (c->ctl_sock[0] != '\0') {
			co += (size_t)snprintf(cmd + co,
				sizeof(cmd) - co, " --sock");
			co = cmd_arg(cmd, sizeof(cmd), co, c->ctl_sock);
		}
		co += (size_t)snprintf(cmd + co, sizeof(cmd) - co,
			" spawn");
		if (c->preload[0] != '\0') {
			co += (size_t)snprintf(cmd + co,
				sizeof(cmd) - co, " --preload");
			co = cmd_arg(cmd, sizeof(cmd), co, c->preload);
		}
		co += (size_t)snprintf(cmd + co, sizeof(cmd) - co,
			" --");
		for (k = 0; k < (int)s->argc; k++)
			co = cmd_arg(cmd, sizeof(cmd), co,
				s->argv[k]);

		proc = popen(cmd, "r");
		if (proc == NULL)
			return;
		out[0] = '\0';
		while (fgets(out, sizeof(out), proc) != NULL)
			;
		if (pclose(proc) != 0 || out[0] != '{')
			return;
		r->spawn_ok = 1;
		{
			JSON_Value *v = json_parse_string(out);

			if (v != NULL) {
				r->pid = (long)json_object_get_number(
					json_value_get_object(v), "pid");
				json_value_free(v);
			}
		}
	}
	if (!r->spawn_ok || r->pid <= 0)
		return;

	/* 3. the departure: the journal's exit record */
	r->done = await_exit(c, r->pid, c->timeout_sec,
		&r->signaled, &r->code) == 0;
	r->elapsed = difftime(time(NULL), t0);
}

/* ---- the pool ------------------------------------------------------------ */

static void *
worker(void *arg)
{
	struct pool *p = (struct pool *)arg;

	for (;;) {
		size_t idx;

		pthread_mutex_lock(&p->mu);
		if (p->next_cell >= p->c->n_cells)
			idx = (size_t)-1;
		else
			idx = p->next_cell++;
		pthread_mutex_unlock(&p->mu);

		if (idx == (size_t)-1)
			break;
		run_cell(p->c, (int)idx, &p->results[idx]);
	}
	return NULL;
}

/* ---- reporting ------------------------------------------------------------ */

static const char *
verdict(const struct run_result *r)
{
	if (!r->push_ok)
		return "PUSH-FAILED";
	if (!r->spawn_ok)
		return "SPAWN-FAILED";
	if (!r->done)
		return "TIMEOUT";
	if (r->signaled)
		return "CRASH";
	if (r->code == 0)
		return "CLEAN";
	return "FAIL";
}

int
main(int argc, char **argv)
{
	struct campaign c;
	char err[256];
	size_t i;

	if (argc != 2) {
		fprintf(stderr,
			"usage: retrace-campaign MANIFEST.json\n");
		return 2;
	}
	if (campaign_load(argv[1], &c, err, sizeof(err)) != 0) {
		fprintf(stderr, "retrace-campaign: %s\n", err);
		return 2;
	}
	if (c.journal_base[0] == '\0' || c.ctl_sock[0] == '\0') {
		fprintf(stderr,
			"retrace-campaign: manifest needs ctl_sock and journal\n");
		campaign_free(&c);
		return 2;
	}

	printf("campaign: %zu samples x %zu policies x %d = %zu runs "
	       "(concurrency %d)\n",
		c.n_samples, c.n_policies, c.repeats, c.n_cells,
		c.concurrency);

	{
		struct run_result *results = calloc(c.n_cells,
			sizeof(*results));
		struct pool p;
		pthread_t th[64];
		int nth = c.concurrency > 64 ? 64 : c.concurrency;
		int rc = 0;

		if (nth < 1)
			nth = 1;
		memset(&p, 0, sizeof(p));
		p.c = &c;
		p.results = results;
		pthread_mutex_init(&p.mu, NULL);

		for (i = 0; i < (size_t)nth; i++)
			pthread_create(&th[i], NULL, worker, &p);
		for (i = 0; i < (size_t)nth; i++)
			pthread_join(th[i], NULL);
		pthread_mutex_destroy(&p.mu);

		/* the triage table */
		printf("%-20s %-16s %4s %8s  %-12s %s\n",
			"SAMPLE", "POLICY", "REP", "PID",
			"VERDICT", "EVIDENCE");
		for (i = 0; i < c.n_cells; i++) {
			struct campaign_cell *cell = &c.cells[i];

			printf("%-20s %-16s %4d %8ld  %-12s "
			       "retrace-ctl events --journal %s "
			       "--query 'pid == %ld'\n",
				c.samples[cell->sample].name,
				c.policies[cell->policy].name,
				cell->repeat, results[i].pid,
				verdict(&results[i]),
				c.journal_base, results[i].pid);
			if (strcmp(verdict(&results[i]), "TIMEOUT") == 0)
				rc = 1;
		}

		/* summary */
		{
			int clean = 0, fail = 0, crash = 0, other = 0;

			for (i = 0; i < c.n_cells; i++) {
				const char *v = verdict(&results[i]);

				if (strcmp(v, "CLEAN") == 0)
					clean++;
				else if (strcmp(v, "FAIL") == 0)
					fail++;
				else if (strcmp(v, "CRASH") == 0)
					crash++;
				else
					other++;
			}
			printf("summary: %zu runs: %d clean, %d fail, "
			       "%d crash, %d other\n",
				c.n_cells, clean, fail, crash, other);
		}
		free(results);
		campaign_free(&c);
		return rc;
	}
}
