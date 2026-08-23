/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * etw2retrace: raw ETW rows (etw-capture.ps1 jsonl) -> retrace
 * trace JSON (TODO.trace-profile/24). The Windows kernel layer
 * (truth) normalizes to retrace's entry shape; retrace-profile
 * --kernel (and retrace-correlate --outside) consume the output
 * like any other stream.
 *
 * The capture script pins the row shape, so the converter never
 * sees Windows-version ETL dialects:
 *   {"time":1692800000.5,"pid":1234,"tid":5678,
 *    "task":"CreateFile","file":"C:\\x\\y.txt",
 *    "detail":"DesiredAccess: GENERIC_READ"}
 *
 * Rows without "file" are skipped (CloseFile carries a
 * FileObject, not a name -- unnameable events carry no grading
 * value). Unknown tasks map through the table below or are
 * skipped.
 */

#ifndef RETRACE_TOOLS_ETW2RETRACE_CONVERT_H_
#define RETRACE_TOOLS_ETW2RETRACE_CONVERT_H_

#include <stdio.h>

#include "parson.h"

int etw_convert(FILE *in, JSON_Array *out);

#endif /* RETRACE_TOOLS_ETW2RETRACE_CONVERT_H_ */
