/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * CLI wrapper only -- the conversion logic lives in convert.c;
 * the shared driver is tools/common/converter.c
 * (TODO.trace-profile/26).
 */

#include "converter.h"
#include "convert.h"

int main(int argc, char **argv)
{
	static const struct converter_app app = {
		.name = "retrace-truss2retrace",
		.usage =
		"Convert `truss -f -o truss.log ./app`\n"
		"output to a retrace trace document (kernel-layer truth).\n",
		.convert = truss_convert,
		.row_noun = "syscall lines",
	};

	return converter_main(argc, argv, &app);
}
