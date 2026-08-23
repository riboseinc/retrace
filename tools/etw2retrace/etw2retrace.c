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
		.name = "retrace-etw2retrace",
		.usage =
		"Convert scripts/win/etw-capture.ps1 raw rows to a retrace\n"
		"trace document (kernel-layer truth).\n",
		.convert = etw_convert,
		.row_noun = "ETW rows",
	};

	return converter_main(argc, argv, &app);
}
