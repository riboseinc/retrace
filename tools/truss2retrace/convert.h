/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_TOOLS_TRUSS2RETRACE_CONVERT_H_
#define RETRACE_TOOLS_TRUSS2RETRACE_CONVERT_H_

#include <stdio.h>

#include "parson.h"

int truss_convert(FILE *in, JSON_Array *out);

#endif /* RETRACE_TOOLS_TRUSS2RETRACE_CONVERT_H_ */
