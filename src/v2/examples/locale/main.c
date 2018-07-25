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

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>


void print_usage(void)
{
	printf("Usage: ctype_exmpl $CAREGORY $LOCALE\n"
		"$CAREGORY: One of the following:\n"
		"\tLC_ALL\n"
		"\tLC_COLLATE\n"
		"\tLC_CTYPE\n"
		"\tLC_MONETARY\n"
		"\tLC_NUMERIC\n"
		"\tLC_TIME\n"
		"\tLC_MESSAGES\n"
		"$LOCALE: required setting of category.\n"
		"e.g. \"locale_exmpl LC_ALL da_DK\"\n"
		"\nRefer to locale.h documentation for additional info.\n"
		);
}


int main(int argc, char *argv[])
{
	int i;
	char *ret;
	struct lconv *p;

	static const struct {
		char *cat_str;
		int cat;
	} cats[] = {
		{"LC_ALL", LC_ALL},
		{"LC_COLLATE", LC_COLLATE},
		{"LC_CTYPE", LC_CTYPE},
		{"LC_MONETARY", LC_CTYPE},
		{"LC_NUMERIC", LC_NUMERIC},
		{"LC_TIME", LC_TIME},
		{"LC_MESSAGES", LC_MESSAGES},
		{"", 0}
	};

	if (argc != 3) {
		print_usage();
		return -1;
	}

	i = 0;
	while (strlen(cats[i].cat_str) && strcmp(cats[i].cat_str, argv[1]))
		i++;
	if (!strlen(cats[i].cat_str)) {
		print_usage();
		return -1;
	}

	ret = setlocale(cats[i].cat, argv[2]);
	printf("setlocale(%d, %s) = %p\n", cats[i].cat,
			argv[2], ret);

	p = localeconv();
	printf("localeconv() = %p\n", p);

	return 0;
}
