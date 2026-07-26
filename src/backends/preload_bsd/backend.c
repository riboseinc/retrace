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
 * preload-bsd backend: FreeBSD/OpenBSD/NetBSD.
 *
 * The trampoline assembly is byte-identical to preload-elf (both ELF);
 * only the bottom.c differs (different printf-vararg parsing and
 * real-impl resolver). Spawn is the same shared helper as preload-elf.
 *
 *
 */

#include <retrace/backend.h>
#include "preload_common.h"

#include <stddef.h>

static int
preload_bsd_probe(struct retrace_engine *eng, const char *target_path)
{
	(void)eng;
	(void)target_path;
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	return 1;
#else
	return 0;
#endif
}

static retrace_pid_t
preload_bsd_spawn(struct retrace_engine *eng,
                   const char *target_path,
                   char *const argv[],
                   char *const envp[])
{
	const char *lib_path;

	(void)eng;

	lib_path = retrace_preload_detect_lib("libretrace_v2.so");
	if (lib_path == NULL)
		return RETRACE_BACKEND_NOT_FOUND;

	return retrace_preload_spawn_common("LD_PRELOAD", lib_path,
	                                     target_path, argv, envp);
}

static const retrace_backend_t preload_bsd_backend = {
	.name        = "preload-bsd",
	.description = "LD_PRELOAD + RTLD_NEXT interposition for FreeBSD, OpenBSD, NetBSD",
	.rank        = RETRACE_BACKEND_RANK_PREFERRED,
	.probe       = preload_bsd_probe,
	.spawn       = preload_bsd_spawn,
	.attach      = NULL,
	.detach      = NULL,
	.translate_frame = NULL,
};

__attribute__((constructor))
static void register_preload_bsd(void)
{
	retrace_backend_register(&preload_bsd_backend);
}
