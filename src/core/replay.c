/*
 * Copyright (c) 2017, [Ribose Inc](https://ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "replay.h"
#include "logger.h"
#include "real_impls.h"

/*
 * The replay slice (TODO.impl/03). All file IO rides the
 * real-impl table (the reentrancy law -- the seams run inside
 * dispatch, under the guard). State is lazy and write-once.
 */

#define REPLAY_LINE_MAX 512
#define REPLAY_RECORDS_MAX 65536

/*
 * IO shims: the real-impl table inside the traced process (the
 * reentrancy law); plain libc when a member is unresolved --
 * which only happens in unit-harness contexts that link the
 * module without the engine boot.
 */
static const char *env_get(const char *name)
{
	return retrace_real_impls.getenv != NULL ?
		retrace_real_impls.getenv(name) : getenv(name);
}

static FILE *file_open(const char *path, const char *mode)
{
	return retrace_real_impls.fopen != NULL ?
		retrace_real_impls.fopen(path, mode) : fopen(path, mode);
}

static void file_close(FILE *f)
{
	if (retrace_real_impls.fclose != NULL)
		retrace_real_impls.fclose(f);
	else
		fclose(f);
}

static size_t file_read(void *ptr, size_t n, FILE *f)
{
	return retrace_real_impls.fread != NULL ?
		retrace_real_impls.fread(ptr, 1, n, f) :
		fread(ptr, 1, n, f);
}

static int g_mode = -1;		/* -1 unresolved */
static FILE *g_out;
static unsigned int g_seed;
static int g_seed_seen;

/* the replay side: the record, in order */
static char (*g_lines)[REPLAY_LINE_MAX];
static size_t g_line_count;
static size_t g_line_next;

static size_t g_matched;
static size_t g_diverged;

static void resolve_mode(void)
{
	const char *out;
	const char *in;

	if (g_mode != -1)
		return;
	g_mode = 0;
	out = env_get("RETRACE_REPLAY_OUT");
	in = env_get("RETRACE_REPLAY_IN");
	if (out != NULL && in != NULL) {
		log_warn("replay: OUT and IN are exclusive; ignoring both");
		return;
	}
	if (out != NULL) {
		g_out = file_open(out, "w");
		if (g_out == NULL) {
			log_warn("replay: cannot open record %s", out);
			return;
		}
		g_mode = 1;
		return;
	}
	if (in != NULL) {
		FILE *f = file_open(in, "rb");
		char buf[8192];
		size_t fill = 0;
		char *big;

		if (f == NULL) {
			log_warn("replay: cannot open record %s", in);
			return;
		}
		big = (char *)retrace_real_impls.malloc(
			REPLAY_LINE_MAX * 16);
		if (big == NULL) {
			file_close(f);
			return;
		}
		big[0] = '\0';
		for (;;) {
			size_t n = file_read(buf, sizeof(buf), f);
			size_t o = strlen(big);

			if (n == 0)
				break;
			if (o + n >= REPLAY_LINE_MAX * 16)
				break;
			memcpy(big + o, buf, n);
			big[o + n] = '\0';
		}
		file_close(f);

		g_lines = retrace_real_impls.malloc(
			(size_t)REPLAY_RECORDS_MAX *
			REPLAY_LINE_MAX);
		if (g_lines != NULL) {
			char *cur = big;

			while (*cur != '\0' &&
			       g_line_count < REPLAY_RECORDS_MAX) {
				char *nl = strchr(cur, '\n');

				if (nl == NULL)
					break;
				*nl = '\0';
				snprintf(g_lines[g_line_count],
					REPLAY_LINE_MAX, "%s", cur);
				g_line_count++;
				cur = nl + 1;
			}
		}
		retrace_real_impls.free(big);
		g_mode = 2;
		return;
	}
}

int retrace_replay_mode(void)
{
	resolve_mode();
	return g_mode > 0 ? g_mode : 0;
}

unsigned int retrace_replay_seed(unsigned int chosen)
{
	resolve_mode();
	if (g_mode == 1 && !g_seed_seen) {
		fprintf(g_out, "seed %u\n", chosen);
		fflush(g_out);
		g_seed_seen = 1;
		g_seed = chosen;
		return chosen;
	}
	if (g_mode == 2 && !g_seed_seen) {
		/* the first record line is the seed */
		if (g_line_count > 0 &&
		    strncmp(g_lines[0], "seed ", 5) == 0) {
			g_seed = (unsigned int)strtoul(g_lines[0] + 5,
				NULL, 10);
			g_line_next = 1;
		} else {
			log_warn("replay: record carries no seed line");
			g_seed = chosen;
		}
		g_seed_seen = 1;
	}
	return g_seed;
}

void retrace_replay_note(const char *func, const char *kind,
	const char *value)
{
	resolve_mode();
	if (g_mode == 1) {
		if (g_out != NULL)
			fprintf(g_out, "%s %s %s\n",
				func != NULL ? func : "?",
				kind != NULL ? kind : "?",
				value != NULL ? value : "?");
		return;
	}
	if (g_mode != 2 || g_lines == NULL)
		return;
	{
		char want[REPLAY_LINE_MAX];

		snprintf(want, sizeof(want), "%s %s %s",
			func != NULL ? func : "?",
			kind != NULL ? kind : "?",
			value != NULL ? value : "?");
		if (g_line_next >= g_line_count) {
			g_diverged++;
			log_warn("replay: drift -- record exhausted at '%s'",
				want);
			return;
		}
		if (strcmp(want, g_lines[g_line_next]) != 0) {
			g_diverged++;
			log_warn("replay: drift #%zu -- recorded '%s' got '%s'",
				g_line_next, g_lines[g_line_next], want);
			g_line_next++;
			return;
		}
		g_matched++;
		g_line_next++;
	}
}

void retrace_replay_report(void)
{
	resolve_mode();
	if (g_mode == 1 && g_out != NULL) {
		fflush(g_out);
		file_close(g_out);
		g_out = NULL;
		log_info("replay: recorded (seed %u)", g_seed);
		return;
	}
	if (g_mode != 2)
		return;
	log_info("replay: %zu matched, %zu diverged, %zu unconsumed",
		g_matched, g_diverged,
		g_line_count > g_line_next ?
			g_line_count - g_line_next : 0);
	if (g_lines != NULL)
		retrace_real_impls.free(g_lines);
	g_lines = NULL;
}
