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
#include "parson.h"

enum Modules {
	ACTIONS,
	CONF,
	DATA_TYPES,
	ENGINE,
	FUNCS,
	MAIN,
	ARCH,
	MODULES_CNT
};

enum Severity {
	SEVERITY_DEBUG,
	SEVERITY_INFO,
	SEVERITY_WARN,
	SEVERITY_ERROR,
	SEVERITY_CNT
};

int retrace_logger_init(void);
void retrace_logger_deinit(void);
void retrace_loger_update_config(void);
void retrace_logger_log(int module, int sev, const char *fmt, ...);
void retrace_logger_log_json(int module, int sev, JSON_Value *msg_value);

#define log_err(fmt, ...) \
        retrace_logger_log(FUNCS, SEVERITY_ERROR, fmt, ##__VA_ARGS__)

#define log_info(fmt, ...) \
        retrace_logger_log(FUNCS, SEVERITY_INFO, fmt, ##__VA_ARGS__)

#define log_warn(fmt, ...) \
        retrace_logger_log(FUNCS, SEVERITY_WARN, fmt, ##__VA_ARGS__)

#define log_dbg(fmt, ...) \
        retrace_logger_log(FUNCS, SEVERITY_DEBUG, fmt, ##__VA_ARGS__)

