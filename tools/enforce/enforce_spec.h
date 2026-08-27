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

#ifndef RETRACE_ENFORCE_SPEC_H_
#define RETRACE_ENFORCE_SPEC_H_

#include <stddef.h>

/*
 * The kernel-enforcement spec (TODO.beyond-libc/01): the
 * deployable artifact `retrace-profile enforce` emits and the
 * installer `retrace-enforce` consumes. One spec, two planes:
 * a Landlock ruleset (path-precise) and a seccomp floor
 * (syscall-coarse). Both optional -- an absent section means
 * that plane is not installed.
 */

#define ENFORCE_RULES_MAX 512
#define ENFORCE_SYSCALLS_MAX 128
#define ENFORCE_AC_PATHS_MAX 256	/* appcontainer grants */

enum enforce_access {
	ENF_READ = 0x01,	/* read files / list dirs */
	ENF_WRITE = 0x02,	/* write/create/remove */
	ENF_EXECUTE = 0x04,	/* exec / mmap-exec (shared libs) */
};

struct enforce_rule {
	char path[512];
	unsigned int access;	/* enum enforce_access bits */
};

struct enforce_syscall {
	char name[32];		/* e.g. "socket" (base name) */
};

struct enforce_spec {
	char sandbox_exec[8192];	/* Seatbelt profile (macOS) */
	struct enforce_rule rules[ENFORCE_RULES_MAX];
	size_t rules_n;
	struct enforce_syscall deny[ENFORCE_SYSCALLS_MAX];
	size_t deny_n;
	int no_new_privs;	/* set PR_SET_NO_NEW_PRIVS (default 1) */
	/* AppContainer (Windows, 01 P1): the coarse kernel plane.
	 * name empty = not compiled into this spec.
	 */
	char ac_name[128];
	char ac_read[ENFORCE_AC_PATHS_MAX][512];
	size_t ac_read_n;
	char ac_write[ENFORCE_AC_PATHS_MAX][512];
	size_t ac_write_n;
};

/*
 * Parse spec JSON:
 * {"landlock":{"rules":[{"path":..,"access":["rd","wr","x"]}]}
 *  "seccomp":{"deny":["socket","ptrace"]}}
 * Unknown fields are ignored (forward compat). Returns 0/-1.
 */
int enforce_spec_parse(struct enforce_spec *spec, const char *json);

#endif /* RETRACE_ENFORCE_SPEC_H_ */
