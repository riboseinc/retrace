/*
 * Copyright (c) 2017, [Ribose Inc](https://ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "journal_sign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "journal.h"

#ifdef RETRACE_HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>

#define SIG_MAX 128

static int sign_head(const char *key_path, const char *msg,
	unsigned char *sig, size_t *sig_len, char *err,
	size_t err_cap)
{
	FILE *kf = fopen(key_path, "r");
	EVP_PKEY *key;
	EVP_MD_CTX *ctx;

	if (kf == NULL) {
		snprintf(err, err_cap, "cannot read key %s", key_path);
		return -1;
	}
	key = PEM_read_PrivateKey(kf, NULL, NULL, NULL);
	fclose(kf);
	if (key == NULL) {
		snprintf(err, err_cap, "bad key %s", key_path);
		return -1;
	}
	ctx = EVP_MD_CTX_new();
	if (ctx == NULL ||
	    EVP_DigestSignInit(ctx, NULL, NULL, NULL, key) != 1 ||
	    EVP_DigestSign(ctx, sig, sig_len,
		    (const unsigned char *)msg, strlen(msg)) != 1) {
		snprintf(err, err_cap, "sign failed");
		EVP_MD_CTX_free(ctx);
		EVP_PKEY_free(key);
		return -1;
	}
	EVP_MD_CTX_free(ctx);
	EVP_PKEY_free(key);
	return 0;
}

int retraced_journal_sign_file(const char *key_path,
	const char *journal_path, char *rec, size_t rec_cap,
	char *err, size_t err_cap)
{
	uint64_t head;
	size_t lines;
	size_t broken;
	char msg[64];
	unsigned char sig[SIG_MAX];
	size_t sig_len = sizeof(sig);
	char enc[256];
	int n;

	if (retraced_journal_chain_file(journal_path, 0, &head, &lines,
		    &broken) != 0) {
		snprintf(err, err_cap,
			"chain broken at line %zu -- refusing to sign",
			broken);
		return -1;
	}
	snprintf(msg, sizeof(msg), "head=%016llx lines=%zu",
		(unsigned long long)head, lines);
	if (sign_head(key_path, msg, sig, &sig_len, err, err_cap) != 0)
		return -1;
	EVP_EncodeBlock((unsigned char *)enc, sig, (int)sig_len);
	n = snprintf(rec, rec_cap,
		"{\"name\":\"retrace.journal.signed\",\"alg\":\"ed25519\",\"key_id\":\"%02x%02x\",\"head\":\"%016llx\",\"lines\":%zu,\"sig\":\"%s\"}",
		sig[0], sig[1], (unsigned long long)head, lines, enc);
	if (n <= 0 || (size_t)n >= (int)rec_cap) {
		snprintf(err, err_cap, "record too long");
		return -1;
	}
	return 0;
}

/* find the LAST line containing the seal marker */
static int read_seal(const char *journal_path, char *seal,
	size_t seal_cap, char *head_hex, size_t head_cap,
	long *lines_sealed)
{
	FILE *f = fopen(journal_path, "r");
	char line[2048];

	seal[0] = '\0';
	if (f == NULL)
		return -1;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, "retrace.journal.signed") != NULL) {
			size_t n = strlen(line);

			if (n >= seal_cap)
				n = seal_cap - 1;
			memcpy(seal, line, n);
			seal[n] = '\0';
		}
	}
	fclose(f);
	if (seal[0] == '\0')
		return -1;
	{
		/* the seal's own fields: head + lines (parse by
		 * markers -- the record is one line, fields fixed)
		 */
		const char *h = strstr(seal, "\"head\":\"");
		const char *l = strstr(seal, "\"lines\":");

		if (h == NULL || l == NULL)
			return -1;
		h += 8;
		snprintf(head_hex, head_cap, "%.*s", 16, h);
		*lines_sealed = strtol(l + 8, NULL, 10);
	}
	return 0;
}

static int pull_b64(const char *seal, unsigned char *sig,
	size_t *sig_len)
{
	const char *s = strstr(seal, "\"sig\":\"");

	if (s == NULL)
		return -1;
	s += 7;
	{
		const char *e = strchr(s, '"');

		if (e == NULL || (size_t)(e - s) / 4 * 3 > SIG_MAX)
			return -1;
		*sig_len = (size_t)EVP_DecodeBlock(sig,
			(const unsigned char *)s, (int)(e - s));
		/* EVP_DecodeBlock decodes the '=' padding as zero
		 * bytes and COUNTS them -- a 64-byte ed25519 sig
		 * came back 66. Trim the padded tail.
		 */
		{
			int pad = 0;

			while (pad < 2 && e - 1 - pad >= s &&
			       *(e - 1 - pad) == '=')
				pad++;
			*sig_len -= (size_t)pad;
		}
	}
	return *sig_len > 0 ? 0 : -1;
}

int retraced_journal_verify_file(const char *journal_path,
	const char *pubkey_path, char *verdict, size_t verdict_cap)
{
	uint64_t head;
	size_t lines;
	size_t broken;
	char seal[2048];
	char head_hex[32];
	long lines_sealed = -1;
	unsigned char sig[SIG_MAX];
	size_t sig_len = 0;
	char msg[64];
	FILE *kf;
	EVP_PKEY *pub;
	EVP_MD_CTX *ctx;

	if (retraced_journal_chain_file(journal_path, 0, &head, &lines,
		    &broken) != 0) {
		snprintf(verdict, verdict_cap,
			"chain broken at line %zu", broken);
		return -1;
	}
	if (lines == 0) {
		snprintf(verdict, verdict_cap, "empty journal");
		return -1;
	}
	if (read_seal(journal_path, seal, sizeof(seal), head_hex,
		    sizeof(head_hex), &lines_sealed) != 0) {
		snprintf(verdict, verdict_cap,
			"no seal (unsigned journal)");
		return -1;
	}
	/* the seal is the FINAL record and its own chain link:
	 * the sealed state is the chain BEFORE it (lines-1)
	 */
	if ((size_t)lines_sealed != lines - 1) {
		snprintf(verdict, verdict_cap,
			"sealed lines %ld != chain %zu",
			lines_sealed, lines - 1);
		return -1;
	}
	snprintf(msg, sizeof(msg), "head=%s lines=%ld", head_hex,
		lines_sealed);
	kf = fopen(pubkey_path, "r");
	if (kf == NULL) {
		snprintf(verdict, verdict_cap, "cannot read pubkey");
		return -1;
	}
	pub = PEM_read_PUBKEY(kf, NULL, NULL, NULL);
	fclose(kf);
	if (pub == NULL) {
		snprintf(verdict, verdict_cap, "bad pubkey");
		return -1;
	}
	if (pull_b64(seal, sig, &sig_len) != 0) {
		EVP_PKEY_free(pub);
		snprintf(verdict, verdict_cap, "seal signature unreadable");
		return -1;
	}
	ctx = EVP_MD_CTX_new();
	if (ctx == NULL ||
	    EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pub) != 1) {
		EVP_MD_CTX_free(ctx);
		EVP_PKEY_free(pub);
		snprintf(verdict, verdict_cap, "verify init failed");
		return -1;
	}
	{
		int rc = EVP_DigestVerify(ctx, sig, sig_len,
			(const unsigned char *)msg, strlen(msg));

		EVP_MD_CTX_free(ctx);
		EVP_PKEY_free(pub);
		if (rc != 1) {
			snprintf(verdict, verdict_cap,
				"signature mismatch");
			return -1;
		}
	}
	/* the sealed head must equal the recomputed chain state
	 * at the seal point (the line BEFORE the seal record)
	 */
	{
		uint64_t head_at_seal;
		size_t n2, b2;
		char want[32];

		if (retraced_journal_chain_file(journal_path,
			    lines - 1, &head_at_seal, &n2, &b2) != 0) {
			snprintf(verdict, verdict_cap,
				"chain broken at line %zu", b2);
			return -1;
		}
		snprintf(want, sizeof(want), "%016llx",
			(unsigned long long)head_at_seal);
		if (strcmp(head_hex, want) != 0) {
			snprintf(verdict, verdict_cap,
				"sealed head %s != chain %s",
				head_hex, want);
			return -1;
		}
	}
	snprintf(verdict, verdict_cap,
		"verified + signed (ed25519, %zu lines)", lines);
	return 0;
}

#else /* !RETRACE_HAVE_OPENSSL */

int retraced_journal_sign_file(const char *key_path,
	const char *journal_path, char *rec, size_t rec_cap,
	char *err, size_t err_cap)
{
	(void)key_path;
	(void)journal_path;
	(void)rec;
	(void)rec_cap;
	snprintf(err, err_cap, "journal signing needs an OpenSSL build");
	return -1;
}

int retraced_journal_verify_file(const char *journal_path,
	const char *pubkey_path, char *verdict, size_t verdict_cap)
{
	(void)journal_path;
	(void)pubkey_path;
	snprintf(verdict, verdict_cap,
		"journal verification needs an OpenSSL build");
	return -1;
}

#endif /* RETRACE_HAVE_OPENSSL */
