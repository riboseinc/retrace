/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_SUPERVISOR_POLICY_SIG_H_
#define RETRACE_SUPERVISOR_POLICY_SIG_H_

#include <stddef.h>

/*
 * Signed policies (TODO.supervisor/05): a POLICY_SET payload (or
 * the --policy file) may be a WRAPPER instead of bare policy JSON:
 *
 *   {"sig":{"alg":"ed25519","key_id":"<hex16>","sig":"<b64>"},
 *    "blob":"<the exact policy JSON, escaped>"}
 *
 * The signature covers the blob STRING BYTES (raw, as serialized --
 * no canonicalization: what was signed is what ships). The agent
 * verifies against the public key pinned via
 * RETRACE_SUPERVISOR_PUBKEY (a PEM path or inline PEM). With a key
 * pinned, a wrapped policy without a valid signature is REFUSED
 * (fail-closed: channel compromise cannot relax policy); bare
 * policies keep working (the local PEERCRED+nonce model). Without a
 * key pinned, signatures are ignored.
 *
 * Returns: 0 = unsigned payload (caller applies as-is, and fills
 * blob from payload), 1 = signed-and-valid (blob filled from the
 * wrapper), -1 = rejected (reason filled when reason_out given).
 */
int retrace_policy_sig_check(const char *payload, char *blob_out,
	size_t blob_cap, char *reason_out, size_t reason_cap);

/*
 * Load + pin the verification key (idempotent). Returns 0 when a
 * key is pinned (or was already), -1 when the env names a key that
 * cannot be loaded. Without OpenSSL the pin fails-closed for
 * wrapped policies only (bare policies pass).
 */
int retrace_policy_sig_init(void);

/* 1 when a key is pinned */
int retrace_policy_sig_pinned(void);

#endif /* RETRACE_SUPERVISOR_POLICY_SIG_H_ */
