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
 * The journal query (TODO.impl/10): the filter expression
 * language over journal segments. filter_dsl.c compiles the
 * expression; the resolver below answers field lookups from
 * each record's JSON (outer envelope: ts/agent/seq/prev;
 * inner "ev" object: the event's own fields). One expression
 * language across the product -- config filters and journal
 * queries share the parser, the tree, and the glob.
 *
 * This binding also carries the plain-libc real_impls table
 * the DSL references (inside the traced process the table is
 * the reentrancy guard; here it is just malloc & friends).
 */

#include "journal.h"
#include "filter_dsl.h"
#include "real_impls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parson.h"

/*
 * the ctl never evaluates against thread contexts: the
 * ThreadContext resolver's caller lookup is unreached here
 */
#include "posix_compat.h"
int retrace_caller_cache_lookup(void *ret_addr, rc_dl_info_t *out)
{
	(void) ret_addr;
	memset(out, 0, sizeof(*out));
	return 0;
}

struct RetraceRealImpls retrace_real_impls;

/* MSVC rejects function-pointer static initializers for some
 * CRT names (snprintf is inline there); bind at first use
 */
void retraced_journal_query_init(void)
{
	static int bound;

	if (bound)
		return;
	bound = 1;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strncmp = strncmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.real_snprintf = snprintf;
}

/* the declared genesis link (first line's "prev") */
static uint64_t segment_genesis_prev(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[2048];
	const char *p;

	if (f == NULL)
		return 0;
	if (fgets(line, sizeof(line), f) == NULL) {
		fclose(f);
		return 0;
	}
	fclose(f);
	p = strstr(line, "\"prev\":\"");
	if (p == NULL)
		return 0;
	return (uint64_t) strtoull(p + 8, NULL, 16);
}

/* ---- the JSON field resolver ------------------------------------------- */

struct json_user {
	JSON_Object *outer;	/* ts, agent, seq, prev */
	JSON_Object *ev;	/* the event's own fields */
};

static int json_field_num(void *user, const char *name,
	long long *out)
{
	struct json_user *u = (struct json_user *) user;
	JSON_Object *o = u->outer;

	if (o != NULL && json_object_has_value(o, name)) {
		*out = (long long) json_object_get_number(o, name);
		return 1;
	}
	o = u->ev;
	if (o != NULL && json_object_has_value(o, name)) {
		*out = (long long) json_object_get_number(o, name);
		return 1;
	}
	return 0;
}

static int json_field_str(void *user, const char *name,
	const char **out)
{
	struct json_user *u = (struct json_user *) user;
	JSON_Object *o = u->outer;

	*out = NULL;
	if (o != NULL && json_object_has_value(o, name)) {
		*out = json_object_get_string(o, name);
		return 1;
	}
	o = u->ev;
	if (o != NULL && json_object_has_value(o, name)) {
		*out = json_object_get_string(o, name);
		return 1;
	}
	return 0;
}

static const struct retrace_filter_resolver json_resolver = {
	.num = json_field_num,
	.str = json_field_str,
};

static long journal_query_file(const char *path,
	const struct retrace_filter_ast *ast,
	void (*sink)(const char *line, void *user), void *user,
	uint64_t start_prev)
{
	FILE *f = fopen(path, "r");
	char line[2048];
	uint64_t prev = start_prev;
	long emitted = 0;

	if (f == NULL)
		return -1;
	while (fgets(line, sizeof(line), f) != NULL) {
		size_t len = strlen(line);
		JSON_Value *v;

		if (len == 0 || line[len - 1] != '\n')
			break;		/* torn tail: stop cleanly */
		v = json_parse_string(line);
		if (v == NULL)
			break;
		{
			JSON_Object *o = json_value_get_object(v);
			const char *prev_str = json_object_get_string(o,
				"prev");
			uint64_t stored_prev;

			if (prev_str == NULL) {
				json_value_free(v);
				fclose(f);
				return -1;	/* malformed line */
			}
			stored_prev = (uint64_t) strtoull(prev_str,
				NULL, 16);
			if (stored_prev != prev) {
				json_value_free(v);
				fclose(f);
				return -1;	/* broken chain */
			}
		}
		prev = retraced_journal_line_hash(line, prev);

		if (ast == NULL) {
			sink(line, user);
			emitted++;
		} else {
			struct json_user ju;

			ju.outer = json_value_get_object(v);
			ju.ev = json_object_get_object(ju.outer, "ev");
			if (retrace_filter_eval_resolved(ast,
					&json_resolver, &ju)) {
				sink(line, user);
				emitted++;
			}
		}
		json_value_free(v);
	}
	fclose(f);
	return emitted;
}

/* ---- the segment walk --------------------------------------------------- */

/*
 * Walk the segment series for `base` oldest -> newest
 * (segments <base>.NNNN in numeric order, then the plain base
 * file when it exists -- the un-rotated single journal). Each
 * line is chain-verified (same arithmetic as replay); matching
 * records are emitted through the sink. `ast` NULL = no
 * filter (every record matches). Returns the number emitted,
 * or -1 on a broken chain / unreadable segment.
 */
long retraced_journal_query(const char *base,
	const struct retrace_filter_ast *ast,
	void (*sink)(const char *line, void *user), void *user)
{
	char p[600];
	long emitted = 0;
	int lo, hi, seq;

	/* the surviving range (retention removes the oldest:
	 * probe, don't assume the series starts at 0)
	 */
	if (retraced_journal_segment_range(base, &lo, &hi) == 0) {
		for (seq = lo; seq <= hi; seq++) {
			long n;

			retraced_journal_segment_path(base, seq, p,
				sizeof(p));
			/* each segment's chain starts from its OWN
			 * declared genesis link (the predecessor's
			 * final head; a pruned predecessor leaves
			 * the declaration, the internal chain still
			 * verifies)
			 */
			n = journal_query_file(p, ast, sink, user,
				segment_genesis_prev(p));
			if (n < 0)
				return -1;
			emitted += n;
		}
		return emitted;
	}

	/* no numbered segments: the plain base file IS the journal */
	if (retraced_journal_segment_exists(base, NULL)) {
		long n = journal_query_file(base, ast, sink, user, 0);

		if (n < 0)
			return -1;
		emitted += n;
	}
	return emitted;
}
