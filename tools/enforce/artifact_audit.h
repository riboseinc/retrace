/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Audited enforcement artifacts (TODO.beyond-libc/01 P2): the
 * audit story -- WHICH filter was in force WHEN. Every
 * retrace-enforce exec appends a hash-chained record binding
 * (timestamp, pid, spec digest, backends, argv); --verify-audit
 * replays the chain. A tampered or truncated trail fails
 * verification, and an explicitly requested trail that cannot
 * be appended fail-closes the exec.
 *
 * Chain discipline mirrors the supervisor journal: each line
 * carries the previous line's hash; the digest is SHA-256 when
 * the build links OpenSSL, FNV-1a-64 otherwise (the "alg" field
 * says which; signatures ride the policy-signing plan).
 */

#ifndef RETRACE_TOOLS_ENFORCE_ARTIFACT_AUDIT_H_
#define RETRACE_TOOLS_ENFORCE_ARTIFACT_AUDIT_H_

#include <stddef.h>

#define ENFORCE_DIGEST_HEX_MAX 65	/* 64 hex + NUL */

/*
 * Digest of the spec artifact (raw file bytes). out holds
 * lowercase hex; alg_out (may be NULL) receives "sha256" or
 * "fnv1a64". Returns 0 on success.
 */
int enforce_spec_digest(const char *spec_json, size_t len,
	char out[ENFORCE_DIGEST_HEX_MAX], const char **alg_out);

/*
 * Append one chained record. path: the audit JSONL. backends:
 * e.g. "landlock+seccomp". argv: the exec'd command line (may
 * be NULL). Returns 0 on success; -1 on I/O error or a broken
 * pre-existing chain (fail-closed: the caller must not exec).
 */
int enforce_audit_append(const char *path, long ts, long pid,
	const char digest[ENFORCE_DIGEST_HEX_MAX],
	const char *alg, const char *backends, char *const argv[]);

/*
 * Replay + verify the chain. Returns the number of intact
 * records (>= 0), or -1 on I/O error, -2 on a broken chain /
 * torn tail. last_hash (may be NULL) receives the head.
 */
long enforce_audit_verify(const char *path,
	char last_hash[ENFORCE_DIGEST_HEX_MAX]);

/* the chain hash of one record (test seam + verify) */
int enforce_audit_hash(const char *prev_hex, const char *payload,
	char out[ENFORCE_DIGEST_HEX_MAX]);

/*
 * Record signatures (the "signed" in signed artifacts): with a
 * private key loaded, every appended record carries an Ed25519
 * signature over the same bytes the chain hash covers. Verify
 * with a public key requires a valid signature on EVERY record
 * (fail-closed: a missing signature fails when a key is set).
 * Returns 0/-1; both are no-ops when built without OpenSSL.
 */
int enforce_audit_set_key(const char *priv_pem_path);
int enforce_audit_set_pubkey(const char *pub_pem_path);
int enforce_audit_signing(void);

#endif /* RETRACE_TOOLS_ENFORCE_ARTIFACT_AUDIT_H_ */
