/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "csv.h"

#include <stdlib.h>
#include <string.h>

enum {
	ST_FIELD_START, /* between fields (record start or after ',') */
	ST_UNQUOTED,	/* inside an unquoted field */
	ST_QUOTED,	/* inside a quoted field */
	ST_QUOTE	/* saw '"' inside a quoted field: "" or close */
};

struct scan_state {
	struct PmCsvRow *row;
	pmcsv_record_cb	 cb;
	void *ctx;
	size_t		 count;
	size_t		 skipped;
	/* Write cursor and current-field start, into row->storage. */
	size_t off;
	size_t start;
	size_t field_len; /* chars pushed into the current field */
};

static void
push(struct scan_state *st, char c)
{
	/* Per-field cap plus room for the field's terminating NUL. */
	if (st->field_len >= PMCSV_FIELD_MAX - 1)
		return;
	if (st->off < sizeof(st->row->storage) - 1) {
		st->row->storage[st->off++] = c;
		st->field_len++;
	}
}

static void
end_field(struct scan_state *st)
{
	if (st->row->count < PMCSV_MAX_FIELDS) {
		if (st->off >= sizeof(st->row->storage))
			st->off = sizeof(st->row->storage) - 1;
		st->row->storage[st->off] = '\0';
		st->row->fields[st->row->count++] = st->row->storage + st->start;
	}
	st->off++; /* the NUL's byte */
}

static void
end_record(struct scan_state *st)
{
	/* Blank line: one empty field and nothing else. */
	if (st->row->count > 1 || st->row->fields[0][0] != '\0') {
		st->cb(st->row, st->ctx);
		st->count++;
	}
	st->row->count = 0;
	st->off = 0;
	st->start = 0;
}

static int
is_rec_end(char c)
{
	return c == '\r' || c == '\n';
}

size_t
pmcsv_scan(const char *text, size_t len, pmcsv_record_cb cb, void *ctx, size_t *skipped)
{
	struct scan_state st;
	size_t		  i = 0;
	int		  state = ST_FIELD_START;

	if (skipped != NULL)
		*skipped = 0;
	if (text == NULL || len == 0 || cb == NULL)
		return 0;

	st.row = (struct PmCsvRow *) malloc(sizeof(*st.row));
	if (st.row == NULL)
		return 0;
	st.cb = cb;
	st.ctx = ctx;
	st.count = 0;
	st.skipped = 0;
	st.off = 0;
	st.start = 0;
	st.field_len = 0;
	st.row->count = 0;

	/* UTF-8 BOM at buffer start only. */
	if (len >= 3 && (unsigned char) text[0] == 0xEF && (unsigned char) text[1] == 0xBB &&
	    (unsigned char) text[2] == 0xBF)
		i = 3;

	for (; i < len; i++) {
		char c = text[i];

		switch (state) {
		case ST_FIELD_START:
			st.start = st.off;
			st.field_len = 0;
			if (c == '"') {
				state = ST_QUOTED;
			} else if (c == ',') {
				end_field(&st);
			} else if (is_rec_end(c)) {
				end_field(&st);
				end_record(&st);
				state = ST_FIELD_START;
				if (c == '\r' && i + 1 < len && text[i + 1] == '\n')
					i++;
			} else {
				push(&st, c);
				state = ST_UNQUOTED;
			}
			break;
		case ST_UNQUOTED:
			if (c == ',') {
				end_field(&st);
				state = ST_FIELD_START;
			} else if (is_rec_end(c)) {
				end_field(&st);
				end_record(&st);
				state = ST_FIELD_START;
				if (c == '\r' && i + 1 < len && text[i + 1] == '\n')
					i++;
			} else {
				push(&st, c);
			}
			break;
		case ST_QUOTED:
			if (c == '"')
				state = ST_QUOTE;
			else
				push(&st, c);
			break;
		case ST_QUOTE:
			if (c == '"') {
				push(&st, '"');
				state = ST_QUOTED;
			} else if (c == ',') {
				end_field(&st);
				state = ST_FIELD_START;
			} else if (is_rec_end(c)) {
				end_field(&st);
				end_record(&st);
				state = ST_FIELD_START;
				if (c == '\r' && i + 1 < len && text[i + 1] == '\n')
					i++;
			} else {
				/* Stray text after a closing quote:
				 * tolerated, appended.
				 */
				push(&st, c);
				state = ST_UNQUOTED;
			}
			break;
		default:
			break;
		}
	}

	/* Trailing record without a final newline. */
	if (state == ST_QUOTED) {
		st.skipped++; /* EOF inside an open quote: dropped */
	} else if (state != ST_FIELD_START || st.off > 0) {
		if (state != ST_FIELD_START)
			end_field(&st);
		else if (st.off > 0) {
			st.start = st.off;
			end_field(&st);
		}
		if (st.row->count > 1 || st.row->fields[0][0] != '\0') {
			cb(st.row, st.ctx);
			st.count++;
		}
	}

	if (skipped != NULL)
		*skipped = st.skipped;
	free(st.row);
	return st.count;
}
