/*
 * Copyright (c) 2017, [Ribose Inc](https://ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_TOOLS_JOURNAL_SIGN_H_
#define RETRACE_TOOLS_JOURNAL_SIGN_H_
#include <stddef.h>

/*
 * Journal signing (TODO.impl/07): hash chains prove LOCAL
 * continuity; ed25519 signatures give an external auditor a
 * trust anchor. At the daemon's graceful close, the signing
 * key seals the chain state (head + line count); verification
 * re-walks the chain and checks the seal. Tampering any byte
 * breaks the chain before it breaks the seal -- and a swapped
 * seal names itself.
 *
 * Keys are PEM ed25519 (openssl genpkey -algorithm ed25519);
 * the public half verifies (openssl pkey -pubout). The
 * signature covers the string "head=<16 hex> lines=<count>".
 * v1 seals the close (rotation intervals chain across
 * segments when TODO.impl/10 lands).
 */

/*
 * Read the journal, verify its chain, and sign the state.
 * Returns 0 and fills `rec` with the journal record to append
 * (the record chain-links itself when written); -1 with a
 * reason in `err` on any failure (bad key, broken chain).
 */
int retraced_journal_sign_file(const char *key_path,
	const char *journal_path, char *rec, size_t rec_cap,
	char *err, size_t err_cap);

/*
 * Verify a signed journal: walk the chain, find the seal as
 * the final record, check that the sealed head matches the
 * chain state at the PREVIOUS line, and verify the ed25519
 * signature with the public key.
 * Returns 0 verified; -1 with a one-line reason in `verdict`.
 */
int retraced_journal_verify_file(const char *journal_path,
	const char *pubkey_path, char *verdict, size_t verdict_cap);

#endif /* RETRACE_TOOLS_JOURNAL_SIGN_H_ */
