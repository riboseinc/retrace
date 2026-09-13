/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_CORE_ACTIONS_TLS_SUMMARY_H_
#define RETRACE_CORE_ACTIONS_TLS_SUMMARY_H_

#include <stddef.h>

/*
 * The pure first-line summarizer for TLS plaintext buffers
 * (TODO.impl/13): bounded by the CALLER-SUPPLIED length (TLS
 * buffers are not NUL-terminated -- an unbounded scan would
 * read past the record). Returns the summary's length, or 0
 * when the buffer holds no printable line (binary payload:
 * the evidence is the byte count, not the bytes).
 */
size_t retrace_tls_first_line(const char *buf, size_t len,
	char *out, size_t cap);

#endif /* RETRACE_CORE_ACTIONS_TLS_SUMMARY_H_ */
