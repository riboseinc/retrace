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

#ifndef RETRACE_TOOLS_RETRACED_CTL_H_
#define RETRACE_TOOLS_RETRACED_CTL_H_

#include <stddef.h>
#include <stdint.h>

#include "registry.h"
#include "journal.h"
#include "protocol.h"

#define MAX_AGENTS 128

/* one accepted agent connection (the frame buffer is the conn's
 * own: field-init on accept, never a whole-struct memset)
 */
struct conn {
	int fd;
	uint8_t buf[RETRACE_RPC_PAYLOAD_MAX + RETRACE_RPC_HEADER_SZ];
	size_t fill;
	char agent_id[RETRACED_AGENT_ID_MAX];
	int helloed;
	int spectator;
};

/*
 * The controller plane (TODO.supervisor/07), extracted as a
 * module (the architecture-review testability candidate): all
 * command state lives here, replies go through an injected sink
 * so the whole command surface is unit-testable without a
 * socket or a daemon.
 */
struct retraced_ctl_ctx {
	/*
	 * policy state (the SSOT: load_policy and the WELCOME
	 * epoch read these too)
	 */
	char *policy_blob;
	long policy_epoch;
	int frozen;
	char *thaw_blob;

	/* collaborators */
	struct conn *conns;
	struct retraced_registry *reg;
	struct retraced_journal *jr;

	/*
	 * Claim scopes for the CURRENT controller peer
	 * (TODO.supervisor/08 P1 / beyond-libc/05). Local UDS
	 * ctl starts with RETRACED_SCOPE_ALL (PEERCRED already
	 * gated the accept); a TLS peer gets the bitmask from
	 * its cert URI SAN. Every mutating command checks the
	 * bit before acting -- least privilege by construction.
	 */
	uint32_t scopes;

	/* the reply sink (one line per command) */
	void (*reply_sink)(const char *line, void *user);
	void *reply_user;
};

void retraced_ctl_set_policy(struct retraced_ctl_ctx *ctx,
	const char *blob, long epoch);

/*
 * One policy-loading seam (the architecture review's B): descend
 * a signed-policy wrapper's blob if present, validate
 * policy.epoch >= 1 and intercept_scripts, and return the blob to
 * hold for pushes. Returns 0/-1. Callers keep transport-specific
 * file IO and pushes.
 */
int retraced_policy_load(const char *text, char **blob_out,
	long *epoch_out);
int retraced_ctl_push_policy(struct retraced_ctl_ctx *ctx,
	const char *blob);
/*
 * the cmd -> claim-bit table behind the scope gate; pure policy,
 * no TLS types -- every transport links the ctl module
 */
uint32_t retraced_tls_scope_for_cmd(const char *cmd);

void retraced_ctl_handle_line(struct retraced_ctl_ctx *ctx,
	char *line);

#endif /* RETRACE_TOOLS_RETRACED_CTL_H_ */
