/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * libFuzzer CUSTOM MUTATOR template (TODO.impl/17): pulls the
 * dictionary from retrace's fuzz_dict FORMAT -- one token per
 * line, '#' comments skipped -- the same file the fuzz_str
 * action consumes, so the workbench's dictionary-driven runs
 * and libFuzzer's search speak ONE vocabulary.
 *
 * build:  clang -g -O1 -fsanitize=fuzzer -o fuzz \
 *             harness.c mutator.c
 * run:    RETRACE_MUTATE_DICT=dictionary.txt ./fuzz corpus/
 *
 * The mutator splices dictionary tokens over random offsets:
 * grammar-aware shape (the tokens know the format's keywords)
 * with libFuzzer's raw-byte exploration between.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DICT_MAX 256
#define TOKEN_MAX 192

static char g_tokens[DICT_MAX][TOKEN_MAX];
static size_t g_ntokens;

static void dict_load(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[TOKEN_MAX];

	if (f == NULL)
		return;
	while (g_ntokens < DICT_MAX &&
	       fgets(line, sizeof(line), f) != NULL) {
		size_t n = strlen(line);

		while (n > 0 && (line[n - 1] == '\n' ||
				 line[n - 1] == '\r'))
			line[--n] = '\0';
		if (n == 0 || line[0] == '#')
			continue;
		memcpy(g_tokens[g_ntokens], line, n + 1);
		g_ntokens++;
	}
	fclose(f);
}

static size_t custom_mutate(uint8_t *data, size_t size,
	size_t max_size, unsigned int seed)
{
	static int seeded;

	(void)seed;
	if (!seeded) {
		const char *p = getenv("RETRACE_MUTATE_DICT");

		if (p != NULL)
			dict_load(p);
		seeded = 1;
	}
	if (g_ntokens == 0 || max_size < 2)
		return LLVMFuzzerMutate(data, size, max_size);
	{
		/* splice one dictionary token at a random offset:
		 * grow the buffer when the token needs room
		 */
		uint32_t r = seed;
		const char *tok = g_tokens[r % g_ntokens];
		size_t tl = strlen(tok);
		size_t off;

		r = r * 1103515245u + 12345u;
		if (size == 0)
			off = 0;
		else
			off = (r >> 8) % size;
		if (off + tl > max_size)
			tl = max_size - off;
		if (tl == 0)
			return size;
		memcpy(data + off, tok, tl);
		if (off + tl > size)
			size = off + tl;
		return size;
	}
}

size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
	size_t max_size, unsigned int seed)
{
	/* half the calls stay raw exploration, half splice
	 * dictionary shape -- the mix libFuzzer cannot find on
	 * its own when the grammar is opaque
	 */
	if ((seed & 1) != 0)
		return custom_mutate(data, size, max_size, seed);
	return LLVMFuzzerMutate(data, size, max_size);
}
