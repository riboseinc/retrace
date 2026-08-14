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

/*
 * retrace version macros — see docs/adr/0006-semantic-versioning.md
 *
 * These macros are the canonical source of the version. CMake reads them
 * via project(VERSION), and `retrace --version` reads them at runtime.
 *
 * ABI stability promise: RETRACE_VERSION_MAJOR changes only on incompatible
 * ABI/API change. See ADR-0006 and ADR-0008.
 */

#ifndef RETRACE_VERSION_H
#define RETRACE_VERSION_H

#define RETRACE_VERSION_MAJOR 2
#define RETRACE_VERSION_MINOR 4
#define RETRACE_VERSION_PATCH 0

#define RETRACE_VERSION_STRING "2.4.0"

#define RETRACE_VERSION_ATLEAST(maj, min, pat)                  \
	(RETRACE_VERSION_MAJOR > (maj) ||                       \
	 (RETRACE_VERSION_MAJOR == (maj) &&                    \
	  RETRACE_VERSION_MINOR > (min)) ||                     \
	 (RETRACE_VERSION_MAJOR == (maj) &&                    \
	  RETRACE_VERSION_MINOR == (min) &&                    \
	  RETRACE_VERSION_PATCH >= (pat)))

#endif /* RETRACE_VERSION_H */
