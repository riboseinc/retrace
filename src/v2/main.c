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
#include <sys/uio.h>
#include <stdlib.h>

#include "real_impls.h"
#include "engine.h"
#include "parson.h"
#include "conf.h"
#include "arch_spec.h"
#include "logger.h"
#include "funcs.h"
#include "actions.h"
#include "data_types.h"

#define log_err(fmt, ...) \
	retrace_logger_log(MAIN, ERROR, fmt, ##__VA_ARGS__)

#define log_info(fmt, ...) \
	retrace_logger_log(MAIN, INFO, fmt, ##__VA_ARGS__)

#define log_warn(fmt, ...) \
	retrace_logger_log(MAIN, WARN, fmt, ##__VA_ARGS__)

#define log_dbg(fmt, ...) \
	retrace_logger_log(MAIN, DEBUG, fmt, ##__VA_ARGS__)

int retrace_inited;

__attribute__((constructor(101)))
static void retrace_main(void)
{
	/* The order of module inits is strict */
	//__asm("int $3;");
	int ret;

	if (retrace_as_init())
		/* cant report error... */
		return;

	if (retrace_real_impls_init())
		/* cant report error... */
		return;

	if (retrace_logger_init())
		/* cant report error... */
		return;

	/* init parson code which is used by various modules */
	json_set_allocation_functions(retrace_real_impls.malloc,
			retrace_real_impls.free);

	ret = retrace_conf_init();
	if (ret) {
		log_err("retrace_conf_init() failed, ret = %d", ret);
		return;
	}

	ret = retrace_engine_init();
	if (ret) {
		log_err("retrace_engine_init() failed, ret = %d", ret);
		return;
	}

	ret = retrace_funcs_init();
	if (ret) {
		log_err("retrace_funcs_init() failed, ret = %d", ret);
		return;
	}

	ret = retrace_datatypes_init();
	if (ret) {
		log_err("retrace_datatypes_init() failed, ret = %d", ret);
		return;
	}

	ret = retrace_actions_init();
	if (ret) {
		log_err("retrace_actions_init() failed, ret = %d", ret);
		return;
	}

	log_info("retrace init success");

	retrace_inited = 1;
}
