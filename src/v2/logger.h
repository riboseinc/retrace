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

enum Modules {
	ACTIONS,
	CONF,
	DATA_TYPES,
	ENGINE,
	FUNCS,
	MAIN,
	MODULES_CNT
};

enum Severity {
	DEBUG,
	INFO,
	WARN,
	ERROR,
	SEVERITY_CNT
};

extern int retrace_sev_ena[SEVERITY_CNT];
extern int retrace_mod_ena[MODULES_CNT];

extern char *retrace_severities[SEVERITY_CNT];
extern char *retrace_module_pref[MODULES_CNT];

int retrace_logger_init(void);

#define retrace_logger_log(module, sev, fmt, ...) \
do { \
	if (retrace_sev_ena[(sev)] && retrace_mod_ena[(module)]) \
		retrace_real_impls.printf("[%s][%s]: " fmt "\n", \
			retrace_module_pref[(module)], \
				retrace_severities[(sev)], ##__VA_ARGS__); \
} while (0)

