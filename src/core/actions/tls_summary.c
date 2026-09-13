/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Pure TLS plaintext summarizer (TODO.impl/13). No engine
 * dependencies: the unit harness compiles this TU alone.
 */

#include <stddef.h>

#include "tls_summary.h"

static int printable_or_space(unsigned char c)
{
	return c == ' ' || c == '\t' || (c >= 0x20 && c < 0x7f);
}

size_t retrace_tls_first_line(const char *buf, size_t len,
	char *out, size_t cap)
{
	size_t i;
	size_t o = 0;

	if (buf == NULL || out == NULL || cap < 2)
		return 0;
	out[0] = '\0';
	if (len == 0)
		return 0;
	/* a leading byte that cannot open a printable line = a
	 * binary payload; say so with nothing (the length rides
	 * the event as its own attribute)
	 */
	for (i = 0; i < len && i < cap - 1; i++) {
		unsigned char c = (unsigned char)buf[i];

		if (c == '\r')
			continue;
		if (c == '\n')
			break;
		if (!printable_or_space(c))
			return 0;
		out[o++] = (char)c;
	}
	if (o == 0)
		return 0;
	out[o] = '\0';
	return o;
}
