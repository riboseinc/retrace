/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "stream.h"

#include <stdlib.h>
#include <string.h>

/*
 * Slice-and-parse: a top-level object starts at '{' at depth 0
 * and ends at the matching '}'. Braces and quotes inside strings
 * are tracked so an entry carrying "func": "a{b\"c" still yields
 * exactly one object. parson needs a NUL-terminated string, so
 * each slice is copied into a scratch buffer reused across
 * entries (worst case one entry spanning the whole input).
 */
size_t
corr_stream_scan(const char *text, size_t len, corr_stream_cb cb, void *ctx, size_t *skipped)
{
	char *buf;
	size_t i, count = 0;
	size_t start = 0;
	int    depth = 0;
	int    in_str = 0;
	int    esc = 0;

	if (skipped != NULL)
		*skipped = 0;
	if (text == NULL || len == 0 || cb == NULL)
		return 0;

	buf = (char *) malloc(len + 1);
	if (buf == NULL)
		return 0;

	for (i = 0; i < len; i++) {
		char c = text[i];

		if (depth == 0) {
			if (c == '{') {
				depth = 1;
				start = i;
				in_str = 0;
				esc = 0;
			}
			continue;
		}
		if (in_str) {
			if (esc) {
				esc = 0;
			} else if (c == '\\') {
				esc = 1;
			} else if (c == '"') {
				in_str = 0;
			}
			continue;
		}
		if (c == '"') {
			in_str = 1;
		} else if (c == '{') {
			depth++;
		} else if (c == '}') {
			depth--;
			if (depth == 0) {
				size_t	    olen = i - start + 1;
				JSON_Value *v;

				memcpy(buf, text + start, olen);
				buf[olen] = '\0';
				v = json_parse_string(buf);
				if (v != NULL) {
					cb(json_value_get_object(v), ctx);
					json_value_free(v);
					count++;
				} else if (skipped != NULL) {
					(*skipped)++;
				}
			}
		}
	}

	free(buf);
	return count;
}
