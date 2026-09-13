/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RETRACE_TOOLS_CTL_JOURNAL_QUERY_H_
#define RETRACE_TOOLS_CTL_JOURNAL_QUERY_H_

#include "filter_dsl.h"

#include <stddef.h>

/*
 * Stream the journal series for `base` (numbered segments, then
 * the plain base file when it exists) through `sink`, keeping
 * only records matching `ast` (NULL = all). Each line is chain-
 * verified on the way through. Returns the emitted count, or
 * -1 on a broken chain / unreadable segment.
 */
/* bind the tool's real_impls table (MSVC-safe lazy binding);
 * call before the first compile or query
 */
void retraced_journal_query_init(void);

long retraced_journal_query(const char *base,
	const struct retrace_filter_ast *ast,
	void (*sink)(const char *line, void *user), void *user);

#endif /* RETRACE_TOOLS_CTL_JOURNAL_QUERY_H_ */
