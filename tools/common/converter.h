/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * converter_main (TODO.trace-profile/26): the shared CLI for
 * the *2retrace converter family. Every kernel-truth converter
 * has the same shape -- [-o out] in, convert, serialize, write,
 * and a converted-rows count on stderr -- so the family shares
 * ONE implementation; a converter is now its convert() plus
 * this table entry (OCP: new converter = no new CLI code).
 */

#ifndef RETRACE_TOOLS_COMMON_CONVERTER_H_
#define RETRACE_TOOLS_COMMON_CONVERTER_H_

#include <stdio.h>

#include "parson.h"

struct converter_app {
	const char *name;      /* e.g. "retrace-ktrace2retrace" */
	const char *usage;     /* capture hint; printed under Usage */
	int (*convert)(FILE *in, JSON_Array *out);
	const char *row_noun;  /* "syscall lines", "ETW rows", ... */
};

int converter_main(int argc, char **argv,
	const struct converter_app *app);

#endif /* RETRACE_TOOLS_COMMON_CONVERTER_H_ */
