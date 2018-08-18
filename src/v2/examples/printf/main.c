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
#include <string.h>

void print_usage(void)
{
	printf("Usage: printf_exmpl $FMT $NUM_OF_VARARGS $VARARGS\n"
		"$FMT: printf format\n"
		"$NUM_OF_VARARGS: number of varargs, max 4\n"
		"$VARARGS: list of varargs\n"
		"e.g. printf_exmpl \"hello "
		"str=%%s num1=%%u num2=%%d num3=%%lu\" 4 \\\"world\\\" 1 2 3\n");
}

int main(int argc, char *argv[])
{
	char *env_var;
	int vargs_cnt;
	long int params[4];
	int i;

	if (argc < 3) {
		print_usage();
		return -1;
	}

	vargs_cnt = atoi(argv[2]);
	if (vargs_cnt > 4) {
		print_usage();
		return -1;
	}

	char *st;

	for (i = 3; i != vargs_cnt + 3; i++) {
		st = argv[i];
		if (*((char*) argv[i]) == '"') {
			/* string, eliminate "" */

			argv[i] += 1;
			st = argv[i];

			*(argv[i] + strlen(argv[i]) - 1) = 0;
			params[i - 3] = (long int) argv[i];
		} else {
			/* int */
			params[i - 3] = atoi(argv[i]);
		}
	}

	switch (vargs_cnt) {

	case 0:
		return printf("%s", argv[1]);

	case 1:
		return printf(argv[1], params[0]);

	case 2:
		return printf(argv[1], params[0], params[1]);

	case 3:
		return printf(argv[1], params[0], params[1], params[2]);

	case 4:
		return printf(argv[1], params[0], params[1], params[2], params[3]);

	default:
		return -1;
	}

	return 0;
}
