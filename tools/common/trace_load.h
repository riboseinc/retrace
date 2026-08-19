/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_TOOLS_TRACE_LOAD_H_
#define RETRACE_TOOLS_TRACE_LOAD_H_

#include "parson.h"

#include <stddef.h>

/*
 * Format-tolerant trace loading for the offline tools
 * (TODO.windows/07). retrace emits either one JSON array
 * document (default) or JSONL (RETRACE_LOGGER_FMT=jsonl); a
 * crashed trace truncates the array tail. This loader accepts
 * every shape and yields the same parsed array in all cases:
 * the downstream tool (audit policy scan, diff normalize)
 * never sees the format.
 *
 * trace_load_file returns an owned JSON array value, or NULL
 * when the file cannot be read. *skipped (optional) receives
 * the count of complete-but-corrupt objects dropped.
 */
JSON_Value *trace_load_file(const char *path, size_t *skipped);

#endif /* RETRACE_TOOLS_TRACE_LOAD_H_ */
