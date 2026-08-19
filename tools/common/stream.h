/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_CORRELATE_STREAM_H_
#define RETRACE_CORRELATE_STREAM_H_

#include "parson.h"

#include <stddef.h>

/*
 * Tolerant retrace-log scanner (TODO.next-level/02).
 *
 * retrace writes ONE JSON array document, one entry per call,
 * leading-comma emission, array closed at exit. A traced process
 * that crashes leaves the tail truncated -- every entry that was
 * fully written is still valid. JSONL (one object per line) is
 * the planned streaming format (TODO.next-level/07) and is
 * accepted here too.
 *
 * The scanner walks the text at brace depth 0, hands every
 * complete top-level object to the callback, and silently drops
 * a truncated final object. Everything between objects -- the
 * array brackets, commas, whitespace, a BOM, CRLF -- is ignored.
 */

/*
 * Called once per parsed entry. The entry (and every string in
 * it) is freed before the next callback: copy anything kept.
 * ctx is passed through untouched.
 */
typedef void (*corr_stream_cb)(JSON_Object *entry, void *ctx);

/*
 * Scan text[0..len). Returns the number of entries handed to cb.
 * If skipped is non-NULL it receives the number of complete
 * objects that failed to parse (a corrupt log, not a truncated
 * one). A truncated trailing object is neither counted nor an
 * error.
 */
size_t corr_stream_scan(const char *text, size_t len, corr_stream_cb cb,
			void *ctx, size_t *skipped);

#endif /* RETRACE_CORRELATE_STREAM_H_ */
