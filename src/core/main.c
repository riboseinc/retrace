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
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/uio.h>
#endif
#include <stdlib.h>

#include "real_impls.h"
#include "engine.h"
#include "win_diag.h"
#include "parson.h"
#include "conf.h"
#include "arch_spec.h"
#include "logger.h"
#include "otlp_live.h"
#include "funcs.h"
#include "actions.h"
#include "data_types.h"
#include "config_cache.h"
#include "call_hash.h"

int retrace_inited;

#if defined(_WIN32)
/*
 * Windows: DllMain is the boot point (TODO.windows/05) -- MSVC
 * has no constructors, and under MinGW a DLL constructor would
 * run after DllMain's hook installation. Idempotent so embedders
 * and tests may also call it.
 */
void retrace_core_boot(void)
#else
__attribute__((constructor(101)))
static void retrace_main(void)
#endif
{
	/* The order of module inits is strict */
	/* __asm("int $3;"); */
	int ret;

	if (retrace_inited)
		return;

	retrace_win_diag("boot-as", NULL, 0);
	if (retrace_as_init())
		/* can't report error... */
		return;

	retrace_win_diag("boot-ri", NULL, 0);
	if (retrace_real_impls_init())
		/* can't report error... */
		return;

	retrace_win_diag("boot-log", NULL, 0);
	if (retrace_logger_init())
		/* can't report error... */
		return;

	retrace_win_diag("boot-hash", NULL, 0);
	if (retrace_call_hash_init())
		log_err("retrace_call_hash_init() failed; "
			"call-hash feature disabled");

	/* init parson code which is used by various modules */
	json_set_allocation_functions(retrace_real_impls.malloc,
			retrace_real_impls.free);

	retrace_win_diag("boot-conf", NULL, 0);
	ret = retrace_conf_init();
	if (ret) {
		log_err("retrace_conf_init() failed, ret = %d", ret);
		return;
	}

	retrace_win_diag("boot-cache", NULL, 0);
	retrace_config_cache_build(retrace_conf);

	retrace_loger_update_config();

	retrace_win_diag("boot-eng", NULL, 0);
	ret = retrace_engine_init();
	if (ret) {
		log_err("retrace_engine_init() failed, ret = %d", ret);
		return;
	}

	retrace_win_diag("boot-funcs", NULL, 0);
	ret = retrace_funcs_init();
	if (ret) {
		log_err("retrace_funcs_init() failed, ret = %d", ret);
		return;
	}

	retrace_win_diag("boot-dt", NULL, 0);
	ret = retrace_datatypes_init();
	if (ret) {
		log_err("retrace_datatypes_init() failed, ret = %d", ret);
		return;
	}

	retrace_win_diag("boot-acts", NULL, 0);
	ret = retrace_actions_init();
	if (ret) {
		log_err("retrace_actions_init() failed, ret = %d", ret);
		return;
	}

	/* TODO.trace-profile/31: otlp-c live streaming. The exporter
	 * spawns its own thread (otlp_live_thread_main) and holds the
	 * permanent reentrance guard for it. Init must come after
	 * logger_init (so log_err can be called from inside
	 * otlp_live_init) and after real_impls_init (so the
	 * rc_thread_create function pointer is populated). It is OK
	 * to call before retrace_inited=1 -- the otlp-c side queues
	 * spans regardless.
	 */
	if (retrace_otlp_live_init() != 0)
		log_err("retrace_otlp_live_init() failed; "
			"otlp-c live streaming disabled");

	ret = retrace_as_init_late();
	if (ret) {
		log_err("retrace_as_init_late() failed, ret = %d", ret);
		return;
	}

	log_dbg("retrace init success");

	retrace_inited = 1;
}

static void hash_print_cb(uint64_t hash, void *ctx)
{
	FILE *out = (FILE *)ctx;

	retrace_real_impls.fprintf(out, "  thread hash: 0x%016llx\n",
		(unsigned long long)hash);
}

#if !defined(_MSC_VER) || defined(__clang__)
__attribute__((destructor))
static void retrace_destructor(void)
{
	if (retrace_call_hash_enabled()) {
		/* Print the final per-thread hashes to stderr so users
		 * (and future fuzz harnesses) can grab them. The
		 * lock-free logger path is already in teardown, so we
		 * write directly via real_impls.
		 */
		retrace_real_impls.fprintf(stderr,
			"retrace: call-hash summary:\n");
		retrace_call_hash_walk(hash_print_cb, stderr);
	}
	retrace_call_hash_deinit();
	retrace_otlp_live_deinit();
	retrace_logger_deinit();
}
#endif /* MSVC has no destructors */
