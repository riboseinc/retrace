/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * procmon2retrace -- procmon CSV export -> retrace JSON
 * (TODO.next-level/06). The outer layer on Windows (ETW/procmon)
 * normalizes to retrace's entry shape so retrace-correlate
 * consumes it like any other outside stream.
 *
 * usage: procmon2retrace <in.csv> [out.json]   (default: stdout)
 *
 * Output is one JSON array document, retrace's emission shape.
 * The first record should be the procmon header row; if it is
 * not recognized, procmon's canonical column order is assumed
 * ("Time of Day","Process Name","PID","Operation","Path",
 * "Result","Detail"). Records without an Operation value are
 * counted as bad and skipped.
 */

#include "convert.h"
#include "csv.h"
#include "parson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

struct ConvState {
	FILE *out;
	int   colmap[PMCOL_COUNT];
	int   entries;
	int   bad_rows;
	int   first;
	int   header_done;
};

static void
default_colmap(int colmap[PMCOL_COUNT])
{
	int i;

	for (i = 0; i < PMCOL_COUNT; i++)
		colmap[i] = i;
}

static void
convert_record(const struct PmCsvRow *row, struct ConvState *st)
{
	JSON_Value *entry;
	char *serialized;
	const char *op;

	op = (st->colmap[PMCOL_OPERATION] >= 0 &&
	      (size_t) st->colmap[PMCOL_OPERATION] < row->count) ?
		     row->fields[st->colmap[PMCOL_OPERATION]] :
		     NULL;
	if (op == NULL || op[0] == '\0') {
		st->bad_rows++;
		return;
	}

	entry = pmconv_entry(row, st->colmap);
	if (entry == NULL) {
		st->bad_rows++;
		return;
	}
	serialized = json_serialize_to_string_pretty(entry);
	if (serialized != NULL) {
		fprintf(st->out, "%s%s\n", st->first ? "" : ",\n", serialized);
		st->first = 0;
		st->entries++;
	}
	json_free_serialized_string(serialized);
	json_value_free(entry);
}

static void
on_record(const struct PmCsvRow *row, void *ctx)
{
	struct ConvState *st = (struct ConvState *) ctx;

	if (!st->header_done) {
		st->header_done = 1;
		if (pmconv_map_header(row, st->colmap) == 0)
			return; /* header consumed, not an event */
		default_colmap(st->colmap);
	}

	convert_record(row, st);
}

int
main(int argc, char **argv)
{
	char *text;
	size_t		 len = 0, skipped = 0;
	FILE *out = stdout;
	struct ConvState st;

	if (argc < 2 || argc > 3) {
		fprintf(stderr, "Usage: procmon2retrace <in.csv> [out.json]\n");
		return 2;
	}

	text = read_file(argv[1], &len);
	if (text == NULL) {
		fprintf(stderr, "procmon2retrace: cannot read %s\n", argv[1]);
		return 2;
	}
	if (argc == 3) {
		out = fopen(argv[2], "w");
		if (out == NULL) {
			fprintf(stderr, "procmon2retrace: cannot write %s\n", argv[2]);
			free(text);
			return 2;
		}
	}

	memset(&st, 0, sizeof(st));
	st.out = out;
	st.first = 1;

	fprintf(out, "[\n");
	(void) pmcsv_scan(text, len, on_record, &st, &skipped);
	fprintf(out, "%s]\n", st.entries > 0 ? "\n" : "");

	fprintf(stderr,
		"entries=%d bad-rows=%d%s\n",
		st.entries,
		st.bad_rows,
		skipped > 0 ? " (truncated final record)" : "");

	if (out != stdout)
		fclose(out);
	free(text);
	return (st.entries > 0) ? 0 : 1;
}
