/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-dtrace2retrace -- the macOS kernel-truth converter
 * (TODO.trace-profile/14). Feeds retrace-profile --kernel (and
 * retrace-correlate --outside) from a dtrace/dtruss file
 * capture; dtrace/dtruss need SIP off (csrutil disable) for
 * system binaries.
 *
 * usage: retrace-dtrace2retrace [-o out.json] dtruss.log
 *
 * Recognized line shape (anything else is skipped):
 *   PID/TSYS  syscall("path\0", 0x0, 0x0)         = 0 0
 * e.g.
 *   84546/0x30d7:  open_nocancel("/etc/hosts\0", 0x0, 0x0) = 0 0
 *
 * dtruss shows C strings with a literal "\0" suffix -- stripped.
 * Name variants normalize to the POSIX names the correlate
 * classifier knows (open_nocancel -> open, stat64 -> stat).
 */

#ifndef RETRACE_TOOLS_DTRACE2RETRACE_CONVERT_H_
#define RETRACE_TOOLS_DTRACE2RETRACE_CONVERT_H_

#include <stdio.h>

#include "parson.h"

int dtrace_convert(FILE *in, JSON_Array *out);

#endif /* RETRACE_TOOLS_DTRACE2RETRACE_CONVERT_H_ */
