/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * TODO.trace-profile/31: this stub replaces src/core/otlp_live.c
 * wherever the otlp-c library is unavailable or the toolchain
 * can't compile the module's C11 stdatomic in core context:
 *   - MSVC: <stdatomic.h> needs /std:c11+extensions that core's
 *     flags don't enable (same reason the log ring is stubbed)
 *   - MinGW/UCRT: otlp-c itself doesn't build there (exporter.c
 *     calls Win32 Sleep() without <windows.h>)
 * These stubs satisfy the core's API: init fails, live streaming
 * is silently off, the rest of retrace is unaffected. The
 * native ports ride the TODO.windows track.
 */

#include "otlp_live.h"

#include <stddef.h>
#include <stdint.h>

int retrace_otlp_live_init(void)
{
	return 0; /* not enabled */
}

void retrace_otlp_live_deinit(void)
{
}

int retrace_otlp_live_emit_json(const char *serialized_json)
{
	(void)serialized_json;
	return 0; /* no-op */
}

void retrace_otlp_live_get_stats(uint64_t *emitted, uint64_t *sent,
	uint64_t *dropped_full, uint64_t *dropped_err)
{
	if (emitted != NULL)
		*emitted = 0;
	if (sent != NULL)
		*sent = 0;
	if (dropped_full != NULL)
		*dropped_full = 0;
	if (dropped_err != NULL)
		*dropped_err = 0;
}
