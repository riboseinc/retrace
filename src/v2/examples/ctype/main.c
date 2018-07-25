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
#include <ctype.h>

void print_usage(void)
{
	printf("Usage: ctype_exmpl $CHAR\n"
		"$CHAR: Character value in decimal to pass to ctype funcs\n"
		"e.g. \"ctype_exmpl 123\"\n");
}

struct {
	int (*ctypef)(int c);
	char *fname;
} ctype_funcs[] = {
	{
		.ctypef = isalnum,
		.fname = "isalnum"
	},
	{
		.ctypef = isalpha,
		.fname = "isalpha"
	},
	{
		.ctypef = isblank,
		.fname = "isblank"
	},
	{
		.ctypef = iscntrl,
		.fname = "iscntrl"
	},
	{
		.ctypef = isdigit,
		.fname = "isdigit"
	},
	{
		.ctypef = isgraph,
		.fname = "isgraph"
	},
	{
		.ctypef = islower,
		.fname = "islower"
	},
	{
		.ctypef = isprint,
		.fname = "isprint"
	},
	{
		.ctypef = ispunct,
		.fname = "ispunct"
	},
	{
		.ctypef = isspace,
		.fname = "isspace"
	},
	{
		.ctypef = isupper,
		.fname = "isupper"
	},
	{
		.ctypef = isxdigit,
		.fname = "isxdigit"
	},
	{
		.ctypef = tolower,
		.fname = "tolower"
	},
	{
		.ctypef = toupper,
		.fname = "toupper"
	},
	{
		.fname = NULL
	}
};

int main(int argc, char *argv[])
{
	int usr_char;
	int i;

	if (argc != 2) {
		print_usage();
		return -1;
	}

	usr_char = atoi(argv[1]);

	while (ctype_funcs[i].fname != NULL) {
		printf("%s(%d) = %d\n", ctype_funcs[i].fname,
			usr_char, ctype_funcs[i].ctypef(usr_char));

		i++;
	}

	return 0;
}
