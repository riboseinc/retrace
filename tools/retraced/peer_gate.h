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
 * Transport-auth gate (TODO.supervisor/08 P0): who may talk to
 * retraced. Peer identity on local sockets (the PEERCRED family),
 * the accept allow-rule, and the constant-time agent-nonce
 * compare. Threat model: docs/threat-model-control-plane.md.
 */

#ifndef RETRACED_PEER_GATE_H
#define RETRACED_PEER_GATE_H

/*
 * Discover the connecting peer's uid on an accepted local socket.
 * Returns 0 and stores the uid, or -1 when the platform offers
 * no credential query (OpenBSD: the sockets are still gated by
 * filesystem mode; the daemon journals and fails open per the
 * liveness doctrine -- see the threat model).
 */
int retraced_peer_uid(int fd, long *uid_out);

/*
 * Whether this platform can query peer credentials at all
 * (OpenBSD cannot). The daemon journals a startup note when
 * not, per the threat model.
 */
int retraced_peer_query_supported(void);

/*
 * The allow rule for accepted connections: the daemon's own
 * euid or root. Everything else is refused and journaled.
 */
int retraced_peer_allowed(long peer_uid, long my_euid);

/*
 * Constant-time equality of the 32-hex agent nonce. Length is
 * compared first (leaking only the length is fine).
 */
int retraced_nonce_matches(const char *presented, const char *expected);

#endif /* RETRACED_PEER_GATE_H */
