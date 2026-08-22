/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */


/*
 * truss2retrace: FreeBSD truss log -> retrace trace JSON
 * (TODO.trace-profile/14). The FreeBSD kernel layer (truth)
 * normalizes to retrace's entry shape; retrace-profile --kernel
 * (and retrace-correlate --outside) consume the output like any
 * other stream.
 *
 * Capture with:
 *   truss -f -o truss.log ./app
 *
 * Recognized line shape (anything else is skipped):
 *   1234: openat(AT_FDCWD,"/a/b",O_RDONLY,00) = 3 (0x0)
 * The first quoted string of a file syscall becomes the path;
 * the raw tail becomes "detail" (flags feed the classifier).
 */

#ifndef RETRACE_TOOLS_KTRACE2RETRACE_CONVERT_H_
#define RETRACE_TOOLS_KTRACE2RETRACE_CONVERT_H_

#include <stdio.h>

#include "parson.h"

int ktrace_convert(FILE *in, JSON_Array *out);

#endif /* RETRACE_TOOLS_KTRACE2RETRACE_CONVERT_H_ */
