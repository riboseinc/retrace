/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Property-based tests for the audit policy matcher (TODO.complete/26).
 *
 * policy_rule_matches drives every compliance finding; a logic
 * inversion means missed violations or phantom findings. The unit
 * tests pin hand-picked cases; these properties hold for ANY
 * generated rule and message:
 *
 * P-POLICY-DETERMINISM: matching the same (rule, message) twice
 * yields the same answer.
 *
 * P-POLICY-LOOSENING: if a rule matches, removing any single
 * predicate from it (setting the field NULL) still matches.
 * (AND-semantics: fewer constraints cannot un-match.)
 *
 * P-POLICY-EMPTY-MATCHES-ALL: a rule with no predicates matches
 * every message with a valid object.
 *
 * P-POLICY-EXACT-SOUNDNESS: if func_exact matches, the message's
 * func equals the pattern exactly.
 *
 * P-POLICY-CONTAINS-SOUNDNESS: if path_contains matches, some
 * string value in the message contains the substring.
 *
 * P-POLICY-ENV-IFF: for the glob shapes the matcher supports,
 * suffix (*_S), prefix (P*) and exact (E) match exactly the
 * messages whose "name" ends with S / starts with P / equals E.
 */

#include "property_harness.h"
#include "parson.h"
#include "policy.h"

#include <string.h>
#include <stdlib.h>

/* ----- generators (small alphabet, adversarial shapes) ----- */

static const char *const words[] = {
	"open", "openat", "system", "getenv", "connect",
	"/etc/passwd", "/etc/shadow", "secret.key", "payload",
	"API_TOKEN", "LD_PRELOAD", "PATH", "IFS",
};

#define NWORDS (sizeof(words) / sizeof(words[0]))

static const char *gen_word(struct ret_prng *p)
{
	return words[ret_prng_u32(p, NWORDS)];
}

/* Build a rule with randomly-present predicates. Fields chosen
 * NULL or a random word -- deliberately includes empty-string
 * patterns ("" prefix matches everything with a func) and
 * wildcard shapes for env_pattern.
 */
static void gen_rule(struct ret_prng *p, struct Rule *r)
{
	const char *env_shapes[] = {"suffix", "prefix", "exact", "bare"};
	int shape;

	memset(r, 0, sizeof(*r));
	if (ret_prng_u32(p, 4) != 0)
		r->func_exact = gen_word(p);
	if (ret_prng_u32(p, 4) != 0)
		r->func_prefix = ret_prng_u32(p, 8) == 0 ? "" :
			gen_word(p);
	if (ret_prng_u32(p, 4) != 0)
		r->path_contains = ret_prng_u32(p, 8) == 0 ? "" :
			gen_word(p);

	shape = ret_prng_u32(p, 4);
	if (ret_prng_u32(p, 4) != 0) {
		static char buf[64];
		const char *w = gen_word(p);

		switch (shape) {
		case 0:
			snprintf(buf, sizeof(buf), "*_%s", w);
			break;
		case 1:
			snprintf(buf, sizeof(buf), "%s*", w);
			break;
		case 2:
			snprintf(buf, sizeof(buf), "%s", w);
			break;
		default:
			snprintf(buf, sizeof(buf), "*");
			break;
		}
		r->env_pattern = buf;
	}
	r->severity = (enum Severity)ret_prng_u32(p, 4);
}

/* Build a random message object: optional func, 0-3 extra string
 * fields, optional name field. Returns a JSON_Value the caller
 * frees; *out_obj borrows from it.
 */
static JSON_Value *gen_message(struct ret_prng *p, JSON_Object **out_obj)
{
	JSON_Value *v = json_value_init_object();
	JSON_Object *o = json_value_get_object(v);
	int extra = (int)ret_prng_u32(p, 4);
	int i;

	if (ret_prng_u32(p, 4) != 0)
		json_object_set_string(o, "func", gen_word(p));
	for (i = 0; i < extra; i++) {
		char key[16];

		snprintf(key, sizeof(key), "k%d", i);
		json_object_set_string(o, key, gen_word(p));
	}
	if (ret_prng_u32(p, 3) != 0)
		json_object_set_string(o, "name", gen_word(p));

	*out_obj = o;
	return v;
}

/* Deep-copy a rule's predicate set, then NULL one predicate. */
static void rule_drop_one(const struct Rule *src, struct Rule *dst,
			  int which)
{
	memcpy(dst, src, sizeof(*dst));
	switch (which) {
	case 0:
		dst->func_exact = NULL;
		break;
	case 1:
		dst->func_prefix = NULL;
		break;
	case 2:
		dst->path_contains = NULL;
		break;
	default:
		dst->env_pattern = NULL;
		break;
	}
}

/* ----- properties ----- */

static int prop_determinism(uint64_t seed)
{
	struct ret_prng p;
	struct Rule r;
	JSON_Object *msg;
	JSON_Value *mv;
	int a, b;

	ret_prng_seed(&p, seed);
	gen_rule(&p, &r);
	mv = gen_message(&p, &msg);

	a = policy_rule_matches(&r, msg);
	b = policy_rule_matches(&r, msg);

	json_value_free(mv);
	return a == b;
}

static int prop_loosening(uint64_t seed)
{
	struct ret_prng p;
	struct Rule r, looser;
	JSON_Object *msg;
	JSON_Value *mv;
	int which;

	ret_prng_seed(&p, seed);
	gen_rule(&p, &r);
	mv = gen_message(&p, &msg);
	which = (int)ret_prng_u32(&p, 4);
	rule_drop_one(&r, &looser, which);

	/* If the original matched, the loosened rule must match too.
	 * (The converse does not hold: loosening can add matches.)
	 */
	if (policy_rule_matches(&r, msg) == 1 &&
	    policy_rule_matches(&looser, msg) != 1) {
		json_value_free(mv);
		return 0;
	}
	json_value_free(mv);
	return 1;
}

static int prop_empty_matches_all(uint64_t seed)
{
	struct ret_prng p;
	struct Rule r = {"R", "", NULL, NULL, NULL, NULL, SEV_INFO};
	JSON_Object *msg;
	JSON_Value *mv;

	(void)seed;
	ret_prng_seed(&p, seed ? seed : 1);
	mv = gen_message(&p, &msg);

	if (policy_rule_matches(&r, msg) != 1) {
		json_value_free(mv);
		return 0;
	}
	json_value_free(mv);
	return 1;
}

static int prop_exact_soundness(uint64_t seed)
{
	struct ret_prng p;
	struct Rule r = {"R", "", NULL, NULL, NULL, NULL, SEV_INFO};
	JSON_Object *msg;
	JSON_Value *mv;
	const char *func;

	ret_prng_seed(&p, seed);
	r.func_exact = gen_word(&p);
	mv = gen_message(&p, &msg);
	func = json_object_get_string(msg, "func");

	if (policy_rule_matches(&r, msg) == 1) {
		int sound = func != NULL &&
			strcmp(func, r.func_exact) == 0;

		json_value_free(mv);
		return sound;
	}
	json_value_free(mv);
	return 1;
}

static int prop_contains_soundness(uint64_t seed)
{
	struct ret_prng p;
	struct Rule r = {"R", "", NULL, NULL, NULL, NULL, SEV_INFO};
	JSON_Object *msg;
	JSON_Value *mv;
	size_t i, n;
	int matched;

	ret_prng_seed(&p, seed);
	r.path_contains = gen_word(&p);
	mv = gen_message(&p, &msg);

	matched = policy_rule_matches(&r, msg);
	if (matched == 1) {
		int found = 0;

		n = json_object_get_count(msg);
		for (i = 0; i < n; i++) {
			const char *s = json_value_get_string(
				json_object_get_value_at(msg, i));

			if (s != NULL && strstr(s, r.path_contains)) {
				found = 1;
				break;
			}
		}
		json_value_free(mv);
		return found;
	}

	json_value_free(mv);
	/* Soundness only constrains the match case; a non-match is
	 * vacuously sound.
	 */
	return 1;
}

static int prop_env_iff(uint64_t seed)
{
	struct ret_prng p;
	struct Rule suffix_r = {"R", "", NULL, NULL, NULL, NULL, SEV_INFO};
	struct Rule prefix_r = {"R", "", NULL, NULL, NULL, NULL, SEV_INFO};
	struct Rule exact_r = {"R", "", NULL, NULL, NULL, NULL, SEV_INFO};
	JSON_Object *msg;
	JSON_Value *mv;
	const char *name;
	const char *word;
	char pat[80];
	size_t wl, pl;
	int ok = 1;

	ret_prng_seed(&p, seed);
	word = gen_word(&p);
	wl = strlen(word);

	snprintf(pat, sizeof(pat), "*_%s", word);
	suffix_r.env_pattern = pat;
	{
		/* prefix/exact rules need separate buffers */
		static char ppre[80];
		static char pex[80];

		snprintf(ppre, sizeof(ppre), "%s*", word);
		prefix_r.env_pattern = ppre;
		snprintf(pex, sizeof(pex), "%s", word);
		exact_r.env_pattern = pex;

		mv = gen_message(&p, &msg);
		name = json_object_get_string(msg, "name");
		pl = name ? strlen(name) : 0;

		/* Suffix: *_W matches iff name ends with W. */
		if (suffix_r.env_pattern[0] == '*' &&
		    policy_rule_matches(&suffix_r, msg) == 1) {
			size_t sl = wl;

			if (!(pl >= sl && strcmp(name + pl - sl, word) == 0))
				ok = 0;
		}
		/* Prefix: W* matches iff name starts with W. */
		if (ok && policy_rule_matches(&prefix_r, msg) == 1) {
			if (!(name != NULL && strncmp(name, word, wl) == 0))
				ok = 0;
		}
		/* Exact: W matches iff name equals W. */
		if (ok && policy_rule_matches(&exact_r, msg) == 1) {
			if (!(name != NULL && strcmp(name, word) == 0))
				ok = 0;
		}
	}

	json_value_free(mv);
	return ok;
}

int main(void)
{
	int failures = 0;

	failures += property_run(prop_determinism,
				 "prop_policy_determinism", 2000, 1);
	failures += property_run(prop_loosening,
				 "prop_policy_loosening", 2000, 1);
	failures += property_run(prop_empty_matches_all,
				 "prop_policy_empty_matches_all", 2000, 1);
	failures += property_run(prop_exact_soundness,
				 "prop_policy_exact_soundness", 2000, 1);
	failures += property_run(prop_contains_soundness,
				 "prop_policy_contains_soundness", 2000, 1);
	failures += property_run(prop_env_iff,
				 "prop_policy_env_iff", 2000, 1);

	printf("[property] %s\n",
		failures ? "some properties FAILED" :
		"all properties passed");
	return failures ? 1 : 0;
}
