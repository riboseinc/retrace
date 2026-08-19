/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-correlate -- the escape-report tool (TODO.next-level/02).
 *
 * Joins an INSIDE stream (a VFS log, retrace-shaped JSON: every
 * path the virtual filesystem saw) against an OUTSIDE stream (a
 * retrace log) and reports outside events that touched paths
 * under --prefix which the inside stream never saw: escapes --
 * operations that bypassed the VFS and hit the host filesystem.
 *
 * Exit codes: 0 no escapes, 1 escapes found, 2 usage or I/O
 * error. Layer honesty: this report only speaks for the layers
 * the captures actually cover -- a libc-boundary outside stream
 * cannot certify the absence of raw-syscall escapes (use the
 * ptrace backend or an outer-layer producer for that layer).
 */

#include "match.h"
#include "stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct InsideState {
	struct CorrSet set;
	size_t	       entries;
};

struct ReportState {
	const char *prefix;
	struct InsideState *inside;
	int		    escapes;
	int		    first;
	int		    json;
};

static void
usage(FILE *out)
{
	fprintf(out,
		"Usage: retrace-correlate --inside <tfs.json> --outside <retrace.json>\n"
		"                         --prefix <path> [--json]\n");
}

static char *
read_file(const char *path, size_t *len_out)
{
	FILE *f = fopen(path, "rb");
	long sz;
	char *buf = NULL;

	if (f == NULL)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0)
		goto fail;
	sz = ftell(f);
	if (sz < 0)
		goto fail;
	if (fseek(f, 0, SEEK_SET) != 0)
		goto fail;
	buf = (char *)malloc((size_t)sz + 1);
	if (buf == NULL)
		goto fail;
	if (fread(buf, 1, (size_t)sz, f) != (size_t)sz)
		goto fail;
	fclose(f);
	buf[sz] = '\0';
	*len_out = (size_t)sz;
	return buf;

fail:
	fclose(f);
	free(buf);
	return NULL;
}

static void
collect_inside(JSON_Object *entry, void *ctx)
{
	struct InsideState *st = (struct InsideState *) ctx;

	st->entries++;
	(void) corr_collect_paths(entry, &st->set);
}

static void
emit(struct ReportState *st, const struct CorrEscape *esc)
{
	if (st->json) {
		printf("%s{\n", st->first ? "" : ",\n");
		printf("  \"path\": \"%s\",\n", esc->path);
		printf("  \"func\": \"%s\",\n", esc->func != NULL ? esc->func : "");
		printf("  \"tid\": %ld\n", esc->tid);
		printf("}");
	} else {
		printf("escape %s func=%s tid=%ld\n",
		       esc->path,
		       esc->func != NULL ? esc->func : "-",
		       esc->tid);
	}
	st->first = 0;
	st->escapes++;
}

static void
report_outside(JSON_Object *entry, void *ctx)
{
	struct ReportState *st = (struct ReportState *) ctx;
	struct CorrEscape   esc;

	if (corr_entry_is_escape(entry, st->prefix, &st->inside->set, &esc))
		emit(st, &esc);
}

int
main(int argc, char **argv)
{
	const char *inside_path = NULL;
	const char *outside_path = NULL;
	const char *prefix_arg = NULL;
	char		   norm_prefix[CORR_PATH_MAX];
	struct InsideState inside;
	struct ReportState report;
	char *inside_text, *outside_text;
	size_t		   inside_len = 0, outside_len = 0;
	size_t		   skipped = 0, inside_paths;
	int		   i, json = 0, rc = 0;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--inside") == 0 && i + 1 < argc)
			inside_path = argv[++i];
		else if (strcmp(argv[i], "--outside") == 0 && i + 1 < argc)
			outside_path = argv[++i];
		else if (strcmp(argv[i], "--prefix") == 0 && i + 1 < argc)
			prefix_arg = argv[++i];
		else if (strcmp(argv[i], "--json") == 0)
			json = 1;
		else {
			usage(stderr);
			return 2;
		}
	}
	if (inside_path == NULL || outside_path == NULL || prefix_arg == NULL) {
		usage(stderr);
		return 2;
	}

	if (corr_normalize(prefix_arg, norm_prefix, sizeof(norm_prefix)) == 0) {
		fprintf(stderr, "retrace-correlate: --prefix is not a path: %s\n", prefix_arg);
		return 2;
	}

	inside_text = read_file(inside_path, &inside_len);
	if (inside_text == NULL) {
		fprintf(stderr, "retrace-correlate: cannot read %s\n", inside_path);
		return 2;
	}
	outside_text = read_file(outside_path, &outside_len);
	if (outside_text == NULL) {
		fprintf(stderr, "retrace-correlate: cannot read %s\n", outside_path);
		free(inside_text);
		return 2;
	}

	corr_set_init(&inside.set);
	inside.entries = 0;
	(void) corr_stream_scan(inside_text, inside_len, collect_inside, &inside, &skipped);
	corr_set_finish(&inside.set);
	inside_paths = inside.set.count;

	memset(&report, 0, sizeof(report));
	report.prefix = norm_prefix;
	report.inside = &inside;
	report.json = json;
	report.first = 1;

	if (report.json)
		printf("[\n");
	(void) corr_stream_scan(outside_text, outside_len, report_outside, &report, &skipped);
	if (report.json)
		printf("%s]\n", report.escapes > 0 ? "\n" : "");

	fprintf(stderr,
		"inside=%zu entries, %zu paths; prefix=%s; escapes=%d%s\n",
		inside.entries,
		inside_paths,
		norm_prefix,
		report.escapes,
		skipped > 0 ? " (entries skipped: corrupt log?)" : "");

	rc = (report.escapes > 0) ? 1 : 0;

	corr_set_free(&inside.set);
	free(inside_text);
	free(outside_text);
	return rc;
}
