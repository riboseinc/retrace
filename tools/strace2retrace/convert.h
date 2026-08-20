/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_STRACE2RETRACE_CONVERT_H_
#define RETRACE_STRACE2RETRACE_CONVERT_H_

#include "parson.h"

#include <stdio.h>

/*
 * Convert an strace -f -e trace=%file log (one syscall per line)
 * into retrace trace entries appended to `out`. Returns the
 * number of converted lines.
 */
int strace_convert(FILE *in, JSON_Array *out);

#endif /* RETRACE_STRACE2RETRACE_CONVERT_H_ */
