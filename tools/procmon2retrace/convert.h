/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_PROCMON_CONVERT_H_
#define RETRACE_PROCMON_CONVERT_H_

#include "csv.h"
#include "parson.h"

/*
 * Procmon CSV row -> retrace JSON entry (TODO.next-level/06).
 *
 * The outer layer on Windows is ETW/procmon; correlation stays
 * format-uniform if its producers normalize to retrace's entry
 * shape. The converter is deliberately offline: retrace-correlate
 * consumes the output like any other stream.
 */

/* Procmon CSV columns, by role. */
enum {
	PMCOL_TOD = 0,	 /* "Time of Day" (wall clock, not epoch) */
	PMCOL_PROCESS,	 /* "Process Name" */
	PMCOL_PID,	 /* "PID" */
	PMCOL_OPERATION, /* "Operation" -> func */
	PMCOL_PATH,	 /* "Path" (often an NT path) */
	PMCOL_RESULT,	 /* "Result" */
	PMCOL_DETAIL,	 /* "Detail" */
	PMCOL_COUNT
};

/*
 * Build colmap from a header row: colmap[PMCOL_*] = field index,
 * or -1 when the column is absent. Header names are matched
 * case-insensitively. Returns 0 if at least Operation mapped,
 * -1 otherwise (row is probably not a procmon header).
 */
int pmconv_map_header(const struct PmCsvRow *header, int colmap[PMCOL_COUNT]);

/*
 * One row -> one retrace-shaped entry:
 *   { "time": 0, "pid": <n>, "tid": 0, "module": "ETW",
 *     "severity": "INFO"|"WARN",
 *     "message": { "func": op, "process": name,
 *                  "time_of_day": tod, "path": path,
 *                  "result": result, "detail": detail } }
 *
 * time is 0 because procmon's timestamp is a wall clock without
 * a date; the original string is preserved in the message. Result
 * SUCCESS maps to INFO, anything else to WARN. Absent columns
 * (colmap -1) are omitted from the message. Returns a value the
 * caller owns, or NULL.
 */
JSON_Value *pmconv_entry(const struct PmCsvRow *row, const int colmap[PMCOL_COUNT]);

#endif /* RETRACE_PROCMON_CONVERT_H_ */
