/*
 * Copyright (c) 2017, [Ribose Inc](https://ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "redact.h"

/*
 * Evidence redaction (TODO.impl/01). The transform is pure C:
 * no libc calls (the agent thread's zero-dispatch law), all
 * state is write-once at set time and read-only after -- safe
 * from any evidence thread.
 *
 * Semantics: patterns match WHOLE TOKENS. A token is a maximal
 * run of [A-Za-z0-9_=.:+@%~-] -- the shapes secrets take in
 * evidence (env names, key=value pairs, path components,
 * Authorization headers). '*' inside a pattern is a run of any
 * characters. Every token in the payload is tested against
 * every pattern; a match becomes "***".
 *
 *   "*_TOKEN"        -> SECRET_TOKEN
 *   "token=x96*"     -> token=x96_file
 *   "AWS_*"          -> AWS_SECRET_ACCESS_KEY
 *   "Authorization=*"-> Authorization=Bearer ey...
 */

#define REDACT_PATTERNS_MAX 32
#define REDACT_PATTERN_LEN 128
#define REDACT_REPLACEMENTS_MAX 64

static char g_patterns[REDACT_PATTERNS_MAX][REDACT_PATTERN_LEN];
static volatile int g_pattern_count;

static size_t xstrlen(const char *s)
{
	size_t n = 0;

	while (s[n] != '\0')
		n++;
	return n;
}

static int is_token_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '_' || c == '=' ||
	       c == '.' || c == ':' || c == '+' || c == '@' ||
	       c == '%' || c == '~' || c == '-';
}

void retrace_redact_set(const char *const *patterns, size_t n)
{
	size_t i;
	int count = 0;

	if (patterns == NULL) {
		g_pattern_count = 0;
		return;
	}
	for (i = 0; i < n && count < REDACT_PATTERNS_MAX; i++) {
		size_t len = patterns[i] != NULL ?
			xstrlen(patterns[i]) : 0;
		size_t j;

		if (len == 0 || len >= REDACT_PATTERN_LEN)
			continue;
		for (j = 0; j < len; j++)
			g_patterns[count][j] = patterns[i][j];
		g_patterns[count][len] = '\0';
		count++;
	}
	g_pattern_count = count;
}

void retrace_redact_set_csv(const char *csv)
{
	const char *p = csv;
	int count = 0;

	if (csv == NULL) {
		g_pattern_count = 0;
		return;
	}
	while (*p != '\0' && count < REDACT_PATTERNS_MAX) {
		const char *comma = p;
		size_t len;
		size_t j;

		while (*comma != '\0' && *comma != ',')
			comma++;
		len = (size_t)(comma - p);
		if (len > 0 && len < REDACT_PATTERN_LEN) {
			for (j = 0; j < len; j++)
				g_patterns[count][j] = p[j];
			g_patterns[count][len] = '\0';
			count++;
		}
		p = *comma == ',' ? comma + 1 : comma;
	}
	g_pattern_count = count;
}

int retrace_redact_active(void)
{
	return g_pattern_count > 0;
}

/* classic whole-string glob: '*' is any run, everything else
 * matches literally. Iterative backtracking, O(len).
 */
static int glob_whole(const char *tok, size_t tok_len,
	const char *pat, size_t pat_len)
{
	size_t t = 0, p = 0, star_t = (size_t)-1, star_p = 0;

	while (t < tok_len) {
		if (p < pat_len && pat[p] == '*') {
			star_t = t;
			star_p = ++p;
		} else if (p < pat_len && pat[p] == tok[t]) {
			t++;
			p++;
		} else if (star_t != (size_t)-1) {
			t = ++star_t;
			p = star_p;
		} else {
			return 0;
		}
	}
	while (p < pat_len && pat[p] == '*')
		p++;
	return p == pat_len;
}

void retrace_redact_apply(char *buf)
{
	int budget = REDACT_REPLACEMENTS_MAX;
	char *cur = buf;

	if (buf == NULL || g_pattern_count == 0)
		return;
	while (*cur != '\0' && budget > 0) {
		char *tok;
		size_t tok_len;
		int hit = 0;
		int p;

		if (!is_token_char(*cur)) {
			cur++;
			continue;
		}
		tok = cur;
		while (is_token_char(*cur))
			cur++;
		tok_len = (size_t)(cur - tok);
		for (p = 0; p < g_pattern_count && !hit; p++) {
			size_t plen = xstrlen(g_patterns[p]);

			if (glob_whole(tok, tok_len, g_patterns[p], plen)) {
				hit = 1;
				break;
			}
		}
		if (hit) {
			/* replace the token with ***: shift the tail
			 * (incl. NUL) left and rescan after it
			 */
			char *dst = tok + 3;
			size_t tail = xstrlen(cur);

			for (size_t j = 0; j <= tail; j++)
				dst[j] = cur[j];
			tok[0] = '*';
			tok[1] = '*';
			tok[2] = '*';
			budget--;
			cur = dst;
		}
	}
}
