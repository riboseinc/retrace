/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * fuzz_dict: the dictionary behind the fuzz_str action
 * (TODO.trace-profile/25). AFL-style: one token per line; '#'
 * lines and blanks are skipped.
 *
 * Grammar templates (TODO.trace-profile/29): a line starting
 * with '@' is a TEMPLATE expanded at load into a real token.
 * '%N%' (N = 1..9) references the Nth FLAT token in file
 * order -- templates reference only flat tokens, never other
 * templates, so cycles are impossible by construction. An out-
 * of-range %N% fails the whole load (author error, loud).
 * Loading and picking are kept free of action machinery so
 * they are unit-testable directly.
 */

#ifndef RETRACE_CORE_ACTIONS_FUZZ_DICT_H_
#define RETRACE_CORE_ACTIONS_FUZZ_DICT_H_

#define FUZZ_DICT_MAX 256
#define FUZZ_DICT_TOKEN_MAX 4096

typedef struct fuzz_dict {
	char tokens[FUZZ_DICT_MAX][FUZZ_DICT_TOKEN_MAX];
	int count;
} fuzz_dict_t;

/*
 * Load tokens from path. Returns 0 on success (even when the
 * file holds fewer tokens than the cap), -1 when the file
 * cannot be opened or read. Truncates tokens longer than
 * FUZZ_DICT_TOKEN_MAX - 1 bytes; stops at FUZZ_DICT_MAX tokens.
 */
int fuzz_dict_load(fuzz_dict_t *d, const char *path);

/*
 * Pick a token with the global RNG (the seed machinery memory_fuzz
 * uses -- retrace_actions_fuzzing_seed_maybe_apply drives the
 * sequence). Returns NULL when the dictionary is empty.
 */
const char *fuzz_dict_pick(const fuzz_dict_t *d);

#endif /* RETRACE_CORE_ACTIONS_FUZZ_DICT_H_ */
