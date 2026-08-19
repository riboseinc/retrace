/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_PROCMON_CSV_H_
#define RETRACE_PROCMON_CSV_H_

#include <stddef.h>

/*
 * Tolerant procmon-CSV scanner (TODO.next-level/06).
 *
 * Procmon's CSV export ("Save as CSV") emits one record per event
 * with a header row:
 *   "Time of Day","Process Name","PID","Operation","Path",
 *   "Result","Detail"
 * Fields are quoted when they contain commas/quotes; the Detail
 * column can contain embedded CRLF and doubled quotes; files may
 * carry a UTF-8 BOM (Excel round-trips). This scanner handles all
 * of it:
 *
 *   - quoted fields: embedded commas, quotes ("" -> "), CR/LF
 *   - record ends: CRLF, LF, or CR (outside quotes)
 *   - BOM at buffer start: skipped
 *   - EOF inside an open quote: record dropped (counted), not
 * fatal -- same tolerance policy as the correlate scanner
 *
 * Rows are delivered synchronously to the callback; the row (and
 * its field storage) is reused for the next record, so the
 * callback must not keep pointers.
 */

#define PMCSV_MAX_FIELDS 16
#define PMCSV_FIELD_MAX 4096

struct PmCsvRow {
	char *fields[PMCSV_MAX_FIELDS]; /* NUL-terminated, unescaped */
	size_t count;
	char   storage[PMCSV_FIELD_MAX * PMCSV_MAX_FIELDS];
};

typedef void (*pmcsv_record_cb)(const struct PmCsvRow *row, void *ctx);

/*
 * Scan text[0..len), invoking cb once per record. Returns the
 * number of records delivered. *skipped (may be NULL) receives
 * the number of records dropped for a missing closing quote.
 * Fields longer than PMCSV_FIELD_MAX are truncated to fit;
 * fields beyond PMCSV_MAX_FIELDS are ignored.
 */
size_t pmcsv_scan(const char *text, size_t len, pmcsv_record_cb cb,
		  void *ctx, size_t *skipped);

#endif /* RETRACE_PROCMON_CSV_H_ */
