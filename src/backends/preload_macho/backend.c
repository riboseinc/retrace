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
 * preload-macho backend: macOS via DYLD_INSERT_LIBRARIES.
 *
 * Per-arch trampolines live in x86_64/ (Phase 3 of the v2 plan adds aarch64/).
 * Spawn uses the shared retrace_preload_spawn_common() helper.
 *
 *
 */

#include <retrace/backend.h>
#include "preload_common.h"

#include <stddef.h>

static int
preload_macho_probe(struct retrace_engine *eng, const char *target_path)
{
	(void)eng;
	(void)target_path;
#ifdef __APPLE__
	return 1;
#else
	return 0;
#endif
}

static retrace_pid_t
preload_macho_spawn(struct retrace_engine *eng,
                     const char *target_path,
                     char *const argv[],
                     char *const envp[])
{
	const char *lib_path;

	(void)eng;

	lib_path = retrace_preload_detect_lib("libretrace_v2.dylib");
	if (lib_path == NULL)
		return RETRACE_BACKEND_NOT_FOUND;

	return retrace_preload_spawn_common("DYLD_INSERT_LIBRARIES", lib_path,
	                                     target_path, argv, envp);
}

static const retrace_backend_t preload_macho_backend = {
	.name        = "preload-macho",
	.description = "DYLD_INSERT_LIBRARIES interposition for macOS (Darwin)",
	.rank        = RETRACE_BACKEND_RANK_PREFERRED,
	.probe       = preload_macho_probe,
	.spawn       = preload_macho_spawn,
	.attach      = NULL,
	.detach      = NULL,
	.translate_frame = NULL,
};

__attribute__((constructor))
static void register_preload_macho(void)
{
	retrace_backend_register(&preload_macho_backend);
}
